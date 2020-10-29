#include <cast_device.hpp>

#include <thread>
#include <chrono>
#include <utility>

const char* source_id = "sender-0";
const char* receiver_id = "receiver-0";

static const char* namespace_connection = "urn:x-cast:com.google.cast.tp.connection";
static const char* namespace_heartbeat = "urn:x-cast:com.google.cast.tp.heartbeat";
static const char* namespace_receiver = "urn:x-cast:com.google.cast.receiver";
static const char* namespace_auth = "urn:x-cast:com.google.cast.tp.deviceauth";

namespace googlecast
{

// Static function to receive a message from a chromecast and parse it into a CastMessage
static bool receive_castmessage(const socketwrapper::SSLTCPSocket& sock, CastMessage& dest_msg)
{
    size_t bytes;
    std::unique_ptr<char[]> pkglen = sock.read<char>(4, &bytes);
    uint32_t len = ntohl(*reinterpret_cast<uint32_t*>(pkglen.get()));

    try {
        if(!dest_msg.ParseFromArray(sock.read<char>(len).get(), len))
            return false;
    } catch(socketwrapper::SocketReadException&) {
        return false;
    }

    return true;
}

// Utility class to periodically receive messages from the chromecast and sort them into namespace related deques
class receiver
{
public:

    receiver() = delete;
    receiver(const receiver&) = delete;
    receiver(receiver&&) = default;
    receiver& operator=(const receiver&) = delete;
    receiver& operator=(receiver&&) = default;

    receiver(const std::shared_ptr<socketwrapper::SSLTCPSocket>& sock)
        : m_sock_ptr(sock) {}

    ~receiver()
    {
        if(m_receive)
            stop();
    }

    void launch()
    {
        m_receive = true;
        m_future = std::async(std::launch::async, [this]() -> void
        {
            while(m_receive)
            {
                if(this->m_sock_ptr->bytes_available())
                {
                    CastMessage msg;
                    if(receive_castmessage(*(this->m_sock_ptr), msg))
                    {
                        // Extract request id from the received message to store it in the message store 
                        try {
                            json payload = json::parse((msg.payload_type() == msg.STRING) ? 
                                msg.payload_utf8() : msg.payload_binary());

                            if(payload.contains("requestId") && payload["requestId"].is_number())
                            {
                                this->m_msg_store.insert({ payload["requestId"], msg });
                            }
                        } catch(nlohmann::json::parse_error&) {}
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void stop()
    {
        m_receive = false;
        m_future.get();
    }

    bool get_msg(uint64_t request_id, CastMessage& dest_msg)
    {
        auto it = m_msg_store.find(request_id);
        if(it == m_msg_store.end())
            return false;

        dest_msg = it->second;
        m_msg_store.erase(it);

        return true;
    }

private:

    std::shared_ptr<socketwrapper::SSLTCPSocket> m_sock_ptr;

    std::future<void> m_future;

    bool m_receive = false;

    std::map<uint64_t, CastMessage> m_msg_store;

};




cast_device::cast_device(const discovery::mdns_res& res, const char* ssl_cert, const char* ssl_key)
    : m_sock_ptr(std::make_shared<socketwrapper::SSLTCPSocket>(AF_INET, ssl_cert, ssl_key))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    m_receiver = std::make_unique<receiver>(m_sock_ptr);

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
    : m_sock_ptr(std::move(other.m_sock_ptr)), m_heartbeat(std::move(other.m_heartbeat)), m_receiver(std::move(other.m_receiver)),
      m_connected(other.m_connected.load()), m_name(std::move(other.m_name)), m_target(std::move(other.m_target)), 
      m_ip(std::move(other.m_ip)), m_txt(std::move(other.m_txt))
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
        m_receiver->stop();
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

    this->send(namespace_connection, R"({ "type": "CONNECT" })");
    m_connected.exchange(true);

    // Launch the receiver to receive and sort messages from the chromecast into the receivers deques
    m_receiver->launch();

    this->send(namespace_receiver, R"({ "type": "GET_STATUS", "requestId": 0 })");
    json recv = read(m_request_id);

    // Launch the heartbeat signal to keep the connection alive
    m_heartbeat = std::async(std::launch::async, [this]() -> void
    {
        while(this->m_connected.load())
        {
            this->send(namespace_heartbeat, R"({ "type": "PING" })");
            // Currently answer to heartbeat are ignored by this client
            std::this_thread::sleep_for(std::chrono::milliseconds(4500));
        }
    });

    return true;
}

bool cast_device::disonnect()
{
    if(!m_connected.load() || !send(namespace_connection, R"({"type": "CLOSE"})"))
        return false;

    m_connected.exchange(false);
    return true;
}

bool cast_device::app_available(std::string_view app_id) const
{
    // TODO
    return true;
}

cast_app& cast_device::launch_app(const std::string& app_id)
{
    if(!m_connected.load())
        throw std::runtime_error("Not connected");

    json j_send, j_recv;

    j_send = R"({ "type": "GET_APP_AVAILABILITY" })"_json;
    j_send["appId"] = { app_id };
    j_send["requestId"] = ++m_request_id;
    send_json(namespace_receiver, j_send);

    j_recv = read(m_request_id);
    if(j_recv.empty() || j_recv["responseType"] != "GET_APP_AVAILABILITY" ||
        j_recv["availability"][app_id.data()] != "APP_AVAILABLE")
        throw std::runtime_error("App not available");

    j_send["type"] = "LAUNCH";
    j_send["appId"] = app_id;
    j_send["requestId"] = ++m_request_id;
    send_json(namespace_receiver, j_send);

    // I think that i dont get a response to my launch request
    // j_recv = read(m_request_id);
    // if(j_recv.empty())
    //     return false;

    // Wait until the chromecast has launched the app succesfully to receive its status
    j_send.erase("appId");
    int poll_it = 0;
    while(true)
    {
        if(poll_it >= 20)
            throw std::runtime_error("Could not bring up app");

        j_send["type"] = "GET_STATUS";
        j_send["requestId"] = ++m_request_id;
        send_json(namespace_receiver, j_send);

        j_recv = read(m_request_id);
        if(j_recv.empty() || !j_recv.contains("status") || !j_recv["status"].contains("applications"))
            continue;

        // Find app to launch in the response
        for(const auto& app_data : j_recv["status"]["applications"])
        {
            if(app_data["appId"] != app_id)
                continue;

            // Found the application we want to launch
            if(m_active_app)
                close_app();

            m_active_app = std::make_unique<cast_app>(this, app_data["appId"], app_data["sessionId"],
                app_data["transportId"], app_data["namespaces"]);
            send(namespace_connection, R"({"type":"CONNECT"})", m_active_app->m_transport_id);
            return *m_active_app;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        poll_it++;
    }

    // TODO on cold start it brekas the while loop immediately and goes to this point ...
    throw std::runtime_error("App not launched");
}

void cast_device::close_app()
{
    if(m_active_app)
    {
        send(namespace_connection, R"({ "type": "CLOSE" })", m_active_app->m_transport_id);
        m_active_app.release();
    }
}

json cast_device::get_status() const
{
    if(!m_connected.load())
        return {};
    
    json obj = json::parse(R"({ "type": "GET_STATUS" })");
    obj["requestId"] = ++m_request_id;
    send_json(namespace_receiver, obj);
    return read(m_request_id);
}

bool cast_device::send(const std::string_view nspace, std::string_view payload, const std::string_view dest_id) const
{
    CastMessage msg;
    msg.set_payload_type(msg.STRING);
    msg.set_protocol_version(msg.CASTV2_1_0);
    msg.set_namespace_(nspace.data());
    msg.set_source_id(source_id);
    msg.set_destination_id(dest_id.data());
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

json cast_device::read(uint32_t request_id) const
{
    uint8_t steps = 0;
    CastMessage msg;
    while(!m_receiver->get_msg(request_id, msg))
    {
        if(steps >= 5)
            return {}; // Return empty json if there is no message

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        steps++;
    }

    try {
        return json::parse((msg.payload_type() == msg.STRING) ? msg.payload_utf8() : msg.payload_binary());
    } catch(nlohmann::json::parse_error&) {
        return {};
    }
}

} // namespace googlecast
