#include <cast_device.hpp>

#include <thread>
#include <chrono>
#include <iostream>

const char* source_id = "sender-0";
const char* receiver_id = "receiver-0";

static const char* namespace_connection = "urn:x-cast:com.google.cast.tp.connection";
static const char* namespace_heartbeat = "urn:x-cast:com.google.cast.tp.heartbeat";
static const char* namespace_receiver = "urn:x-cast:com.google.cast.receiver";
static const char* namespace_auth = "urn:x-cast:com.google.cast.tp.deviceauth";

namespace googlecast
{

cast_device::cast_device(const mdns::mdns_res& res, const char* ssl_cert, const char* ssl_key)
    : m_sock_ptr(std::make_unique<socketwrapper::SSLTCPSocket>(AF_INET, ssl_cert, ssl_key))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    for(const auto& rec : res.records)
    {
        switch(rec.type)
        {
            case 1:
                // A record
                parse_a_record(rec, this->m_ip);
                break;
            case 5:
                // CNAME record
                break;
            case 12:
                // PTR record
                parse_ptr_record(rec, this->m_name);
                break;
            case 16:
                // TXT record
                parse_txt_record(rec, this->m_txt);
                break;
            case 28:
                // AAAA record
                break;
            case 33:
                // SRV record
                parse_srv_record(rec, this->m_port, this->m_target);
                break;
            default:
                // Skip all not expected record types
                break;
        }
    }
}

cast_device::cast_device(cast_device&& other)
    : m_sock_ptr(std::move(other.m_sock_ptr)), m_heartbeat(std::move(other.m_heartbeat)), m_connected(other.m_connected.load()),
      m_name(std::move(other.m_name)), m_target(std::move(other.m_target)), m_ip(std::move(other.m_ip)), 
      m_txt(std::move(other.m_txt))
{
    other.m_connected.exchange(false);
    other.m_request_id = 0;
    other.m_port = 0;
}

cast_device::~cast_device()
{
    if(m_connected.load())
    {
        m_connected.exchange(false);
        m_heartbeat.get();
        send(namespace_connection, "", R"({ "type": "CLOSE"})");
    }
}

bool cast_device::connect()
{
    if(m_connected.load())
        return false;

    try {
        m_sock_ptr->connect(m_port, m_ip);
    } catch(socketwrapper::SocketConnectingException& e) {
        return false;
    }

    this->send(namespace_connection, "", 
        R"({ "type": "CONNECT" })");

    this->send(namespace_receiver, "", 
        R"({ "type": "GET_STATUS", "requestId": 0 })");

    CastMessage msg;
    if(!this->receive(msg))
        return false;
    
    m_connected.exchange(true);
    
    // Launch the heartbeat signal to keep the connection alive
    m_heartbeat = std::async(std::launch::async, [this]() -> void
    {
        while(this->m_connected.load())
        {
            this->send(namespace_heartbeat, "", R"({ "type": "PING" })");

            // CastMessage msg;
            // if(!this->receive(msg))
            //     continue;

            // if(msg.payload_type() != 0 || msg.payload_utf8() != R"({"type":"PONG"})")
            // {
            //     if(msg.namespace_() == namespace_connection && msg.payload_type() == 0 &&
            //         msg.payload_utf8() == R"({"type":"CLOSE"})")
            //         this->m_connected.exchange(false);
            //         continue;
            // }

            std::this_thread::sleep_for(std::chrono::milliseconds(4500));
        }
    });

    return true;
}

bool cast_device::disonnect()
{
    if(!m_connected.load() || 
        !send(namespace_connection, "", R"({"type": "CLOSE"})"))
        return false;

    m_connected.exchange(false);
    return true;
}

bool cast_device::launch_app(const std::string_view app_id)
{
    if(!m_connected.load())
        return false;

    json j_send = R"({ "type": "GET_APP_AVAILABILITY" })"_json;
    j_send["appId"] = { app_id };
    j_send["requestId"] = ++m_request_id;
    send_json(namespace_receiver, "", j_send);

    json j_recv;
    if(!receive_payload(j_recv) || j_recv["responseType"] != "GET_APP_AVAILABILITY" ||
        j_recv["availability"][app_id.data()] != "APP_AVAILABLE")
        return false;
    
    j_send["type"] = "LAUNCH";
    j_send["appId"] = app_id;
    j_send["requestId"] = ++m_request_id;
    send_json(namespace_receiver, "", j_send);

    if(!receive_payload(j_recv))
        return false;
    
    // Wait until the chromecast has launched the app succesfully to receive its status
    j_send.erase("appId");
    int poll_it = 0;
    while(true)
    {
        if(poll_it >= 20)
            return false;

        j_send["type"] = "GET_STATUS";
        j_send["requestId"] = ++m_request_id;
        send_json(namespace_receiver, "", j_send);

        if(receive_payload(j_recv) && j_recv.contains("status") && 
            j_recv["status"].contains("applications"))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        poll_it++;
    }

    // Find the application in the response abject
    for(const auto& app_data : j_recv["status"]["applications"])
    {
        if(app_data["appId"] != app_id)
            continue;

        std::cout << "[DEBUG]: " << app_data << std::endl;

        // TODO parse app data and create interface to interact with the app
        // cast_app app { 
        //     app_data["id"],  
        //     app_data["transportId"],
        //     app_data["sessionId"],
        //     app_data["namespaces"]
        // };

        return true;
    }

    return false;
}

bool cast_device::send(const std::string_view nspace, const std::string_view dest_id, std::string_view payload) const
{
    CastMessage msg;
    msg.set_payload_type(msg.STRING);
    msg.set_protocol_version(msg.CASTV2_1_0);
    msg.set_namespace_(nspace.data());
    msg.set_source_id(source_id);
    msg.set_destination_id((dest_id.empty()) ? receiver_id : dest_id.data());
    msg.set_payload_utf8(payload.data());

    uint32_t len = msg.ByteSize();

    std::unique_ptr<char[]> data = std::make_unique<char[]>(4 + len);
    *reinterpret_cast<uint32_t*>(data.get()) = htonl(len);

    if(!msg.SerializeToArray(&data[4], len))
        return false;

    try {
        m_sock_ptr->write<char>(data.get(), 4 + len);
    } catch(socketwrapper::SocketWriteException& e) {
        return false;
    }

    return true;
}

bool cast_device::receive(CastMessage& dest_msg) const
{
    size_t bytes;
    std::unique_ptr<char[]> pkglen = m_sock_ptr->read<char>(4, &bytes);
    uint32_t len = ntohl(*reinterpret_cast<uint32_t*>(pkglen.get()));

    std::unique_ptr<char[]> msg_buf;
    try {
        msg_buf = m_sock_ptr->read<char>(len);
    } catch(socketwrapper::SocketReadException& e) {
        return false;
    }
    
    if(!dest_msg.ParseFromArray(msg_buf.get(), len))
        return false;

    // Ignore all PONG receives TEMPORARY 
    if(dest_msg.namespace_() == namespace_heartbeat)
        receive(dest_msg);
    
    return true;
}

bool cast_device::receive_payload(json& dest_payload) const
{
    CastMessage msg;
    if(!receive(msg))
        return false;

    if(msg.payload_type() != 0)
        return false;

    dest_payload = json::parse(msg.payload_utf8());
    return true;
}

} // namespace googlecast
