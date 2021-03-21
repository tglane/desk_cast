#include "cast_device.hpp"

#include <thread>
#include <chrono>
#include <utility>

using namespace std::chrono_literals;

static constexpr const char* source_id = "sender-0";
static constexpr const char* receiver_id = "receiver-0";

static constexpr const char* namespace_connection = "urn:x-cast:com.google.cast.tp.connection";
static constexpr const char* namespace_heartbeat = "urn:x-cast:com.google.cast.tp.heartbeat";
static constexpr const char* namespace_receiver = "urn:x-cast:com.google.cast.receiver";
static constexpr const char* namespace_auth = "urn:x-cast:com.google.cast.tp.deviceauth";

namespace googlecast
{

cast_device::cast_device(const discovery::mdns_res& res, std::string_view ssl_cert, std::string_view ssl_key)
    : m_keypair {ssl_cert, ssl_key}
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    for(const auto& rec : res.records)
    {
        switch(rec.type)
        {
            case 1:
                // A record
                parse_a_record(rec, m_ip);
                break;
            case 5:
                // CNAME record
                break;
            case 12:
                // PTR record
                parse_ptr_record(rec, m_name);
                break;
            case 16:
                // TXT record
                parse_txt_record(rec, m_txt);
                break;
            case 28:
                // AAAA record
                break;
            case 33:
                // SRV record
                parse_srv_record(rec, m_port, m_target);
                break;
            default:
                // Skip all not expected record types
                break;
        }
    }
}

cast_device::cast_device(cast_device&& other)
   : m_keypair {std::move(other.m_keypair)}, m_sock {std::move(other.m_sock)}, m_heartbeat {std::move(other.m_heartbeat)}, m_recv_loop {std::move(other.m_recv_loop)},
     m_msg_store {std::move(other.m_msg_store)}, m_active_app {std::move(other.m_active_app)},
     m_connected {other.m_connected.load()}, m_name {std::move(other.m_name)}, m_target {std::move(other.m_target)}, 
     m_txt{std::move(other.m_txt)}, m_ip {std::move(other.m_ip)}, m_port {other.m_port}, m_request_id {other.m_request_id}
{

    other.m_connected.exchange(false);
    other.m_request_id = 0;
    other.m_port = 0;
}

cast_device& cast_device::operator=(cast_device&& other)
{
    if(this != &other)
    {
        m_keypair = std::move(other.m_keypair);
        m_sock = std::move(other.m_sock);
        m_heartbeat = std::move(other.m_heartbeat);
        m_recv_loop = std::move(other.m_recv_loop);
        m_msg_store = std::move(other.m_msg_store);
        m_active_app = std::move(other.m_active_app);
        m_connected.exchange(other.m_connected.load());
        m_name = std::move(other.m_name);
        m_target = std::move(other.m_target);
        m_txt = std::move(other.m_txt);
        m_ip = std::move(other.m_ip);
        m_port = other.m_port;
        m_request_id = other.m_request_id;

        other.m_connected.exchange(false);
        other.m_request_id = 0;
        other.m_port = 0;
    }
    
    return *this;
}

cast_device::~cast_device()
{
    if(m_connected.load())
    {
        m_connected.exchange(false);
        m_recv_loop.get();
        m_heartbeat.get();
        send(namespace_connection, "", R"({ "type": "CLOSE"})");
    }
}

bool cast_device::connect()
{
    if(m_connected.load())
        return false;

    // Connecto on socket level
    m_sock = std::make_unique<net::tls_connection<net::ip_version::v4>>(m_keypair.cert_path, m_keypair.key_path, m_ip, m_port);

    send(namespace_connection, R"({ "type": "CONNECT" })");
    m_connected.exchange(true);

    m_recv_loop = std::async(std::launch::async, [this]()
    {
        while(this->m_connected.load())
        {
            try {
                cast_message msg;

                // TODO Continue reading until len was read -> do-while
                uint32_t len;
                this->m_sock->read(net::span {&len, 1});
                len = ntohl(len);
                std::array<char, 4096> buffer;
                size_t br =  this->m_sock->read(net::span {buffer.data(), len});

                if(len > 0 && msg.ParseFromArray(buffer.data(), len))
                {

                    json payload = json::parse((msg.payload_type() == msg.STRING) ?
                        msg.payload_utf8() : msg.payload_binary());

                    // std::cout << "Received : " << payload["requestId"] << std::endl;
                    // std::cout << payload.dump(2) << "--------------------------\n";

                    // Check if message contains requestId because we dont want to store other messages
                    if(payload.contains("requestId") && payload["requestId"].is_number())
                    {
                        auto msg_iter = this->m_msg_store.find(payload["requestId"]);
                        if(msg_iter != this->m_msg_store.end())
                        {
                            msg_iter->second.second = payload;
                            msg_iter->second.first->notify_all();
                        }
                    }
                }

                std::this_thread::sleep_for(500ms);
            } catch(std::runtime_error& e) {
                std::cout << e.what() << std::endl;
            } catch(json::parse_error& e) {
                std::cout << e.what() << std::endl;
            }
        }
    });

    json recv = send(namespace_receiver, R"({ "type": "GET_STATUS", "requestId": 0 })");

    // Launch the heartbeat signal to keep the connection alive
    m_heartbeat = std::async(std::launch::async, [this]() -> void
    {
        while(this->m_connected.load())
        {
            this->send(namespace_heartbeat, R"({ "type": "PING" })");
            std::this_thread::sleep_for(4500ms);
        }
    });

    return true;
}

bool cast_device::disconnect()
{
    if(!m_connected.load() || !send(namespace_connection, R"({"type": "CLOSE"})"))
        return false;

    m_connected.exchange(false);
    return true;
}

bool cast_device::app_available(std::string_view app_id) const
{
    json obj = json::parse(R"({"type":"GET_APP_AVAILABILITY"})");
    obj["appId"] = {app_id};
    obj["requestId"] = ++m_request_id;

    json recv = send_recv(namespace_receiver, obj);

    if(!recv.empty() && recv["responseType"] == "GET_APP_AVAILABILITY" && 
        recv["availability"][app_id.data()] == "APP_AVAILABLE")
        return true;

    return false;
}

bool cast_device::launch_app(std::string_view app_id, json&& launch_payload)
{
    if(!m_connected.load() || !app_available(app_id))
        return false;

    json j_send, j_recv;

    j_send["type"] = "LAUNCH";
    j_send["appId"] = app_id;
    j_send["requestId"] = ++m_request_id;

    j_recv = send_recv(namespace_receiver, j_send);
    if(j_recv.empty() || j_recv.contains("status") || j_recv["status"].contains("applications"))
    {
        for(const auto& app_data : j_recv["status"]["applications"])
        {
            if(app_data["appId"] != app_id)
                continue;

            // Found the application we want to launch so create the app_details object
            m_active_app = app_details {
                std::string {app_id.begin(), app_id.end()},
                std::move(app_data["sessionId"]),
                std::move(app_data["transportId"]),
                std::move(app_data["namespaces"])
            };

            send(namespace_connection, R"({"type":"CONNECT"})", m_active_app.transport_id);

            // TODO Maybe dont always use the media namespace? Find a way to choose the namespace from the namespaces of the app_details
            uint64_t req_id = ++m_request_id;
            launch_payload["requestId"] = req_id;
            j_recv = send_recv("urn:x-cast:com.google.cast.media", launch_payload, m_active_app.transport_id);

            if(j_recv.contains("type") && j_recv["type"] == "MEDIA_STATUS") 
            {
                return true;
            } 
            else
            {
                m_active_app.clear();
                return false;
            }
        }
    }

    return false;
}

void cast_device::close_app()
{
    if(m_active_app)
        send(namespace_connection, R"({ "type": "CLOSE" })", m_active_app.transport_id);
}

bool cast_device::set_volume(double level)
{
    if(!m_connected)
        return false;

    json msg;
    msg["type"] = "SET_VOLUME";
    msg["requestId"] = ++m_request_id;
    msg["volume"] = json {"level", level};

    send_json(namespace_receiver, msg);
    return true;
}

bool cast_device::set_muted(bool muted)
{
    if(!m_connected)
        return false;

    json msg;
    msg["type"] = "SET_VOLUME";
    msg["requestId"] = ++m_request_id;
    msg["volume"] = json {"muted", muted};

    send_json(namespace_receiver, msg);
    return true;
}

json cast_device::get_status() const
{
    if(!m_connected.load())
        return {};
    
    json obj = json::parse(R"({ "type": "GET_STATUS" })");
    obj["requestId"] = ++m_request_id;
    return send_recv(namespace_receiver, obj);
}

bool cast_device::send(const std::string_view nspace, std::string_view payload, const std::string_view dest_id) const
{
    cast_message msg;
    msg.set_payload_type(msg.STRING);
    msg.set_protocol_version(msg.CASTV2_1_0);
    msg.set_namespace_(nspace.data());
    msg.set_source_id(source_id);
    msg.set_destination_id(dest_id.data());
    msg.set_payload_utf8(payload.data());

    uint32_t len = msg.ByteSize();

    std::vector<char> data;
    data.resize(4 + len);
    *reinterpret_cast<uint32_t*>(data.data()) = htonl(len);

    if(!msg.SerializeToArray(&data[4], len))
        return false;

    try {
        m_sock->send(net::span {data});
    } catch(std::runtime_error& e) {
        return false;
    }

    return true;
}

json cast_device::send_recv(const std::string_view nspace, const json& payload, const std::string_view dest_id) const
{
    json recv;
    cast_message msg;
    msg.set_payload_type(msg.STRING);
    msg.set_protocol_version(msg.CASTV2_1_0);
    msg.set_namespace_(nspace.data());
    msg.set_source_id(source_id);
    msg.set_destination_id(dest_id.data());
    msg.set_payload_utf8(payload.dump());

    int64_t req_id = (payload.contains("requestId") && payload["requestId"].is_number()) ? 
        static_cast<int64_t>(payload["requestId"]) : -1;

    uint32_t len = msg.ByteSize();
    std::vector<char> data;
    data.resize(4 + len);
    *reinterpret_cast<uint32_t*>(data.data()) = htonl(len);

    if(!msg.SerializeToArray(&data[4], len))
        return recv;

    m_msg_store[req_id] = std::make_pair<cond_ptr, json>(std::make_unique<std::condition_variable>(), json {});
    const auto& msg_pair = m_msg_store[req_id];

    try {
        m_sock->send(net::span {data});
    } catch(std::runtime_error& e) {
        return recv;
    }

    if(std::unique_lock<std::mutex> lock {m_msg_mutex}; req_id != -1 && msg_pair.first->wait_for(lock, 5000ms) == std::cv_status::no_timeout)
    {
        recv = msg_pair.second;
        m_msg_store.erase(req_id);
    }

    return recv;
}

} // namespace googlecast

