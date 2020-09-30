#include <cast_device.hpp>

#include <thread>
#include <chrono>
#include <iostream>

using extensions::core_api::cast_channel::CastMessage;

const char* source_id = "sender-0";
const char* receiver_id = "receiver-0";

const char* namespace_connection = "urn:x-cast:com.google.cast.tp.connection";
const char* namespace_heartbeat = "urn:x-cast:com.google.cast.tp.heartbeat";
const char* namespace_receiver = "urn:x-cast:com.google.cast.receiver";
const char* namespace_auth = "urn:x-cast:com.google.cast.tp.deviceauth";

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
        send(namespace_connection, "", 
            R"({ "type": "CLOSE"})");
        
        m_heartbeat.get();
        m_connected.exchange(false);
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
    
    std::cout << "[DEBUG]: " << msg.namespace_() << std::endl;
    std::cout << "[DEBUG]: " << msg.payload_type() << std::endl;
    std::cout << "[DEBUG]: " << msg.payload_utf8() << std::endl;
    std::cout << "----------" << std::endl;

    m_heartbeat = std::async(std::launch::async, [this]() -> void
    {
        while(this->m_connected.load())
        {
            this->send(namespace_heartbeat, "", R"({ "type": "PING" })");
            CastMessage msg;
            if(!this->receive(msg))
                continue;

            if(msg.payload_type() != 0 || msg.payload_utf8() != R"({"type":"PONG"})")
            {
                if(msg.namespace_() == namespace_connection && msg.payload_type() == 0 &&
                    msg.payload_utf8() == R"({"type":"CLOSE"})")
                    this->m_connected.exchange(false);
                    continue;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }
    });

    while(true)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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

    std::cout << "sent" << std::endl;
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
    
    return true;
}
