#ifndef CAST_DEVICE_HPP
#define CAST_DEVICE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <map>
#include <future>
#include <atomic>

#include "../protos/cast_channel.pb.h"
#include <socketwrapper/SSLTCPSocket.hpp>
#include <json.hpp>
#include <mdns_discovery.hpp>
#include <cast_app.hpp>

using extensions::core_api::cast_channel::CastMessage;
using nlohmann::json;

namespace googlecast
{

class cast_device
{
public:

    cast_device() = delete;
    cast_device(const cast_device&) = delete;
    cast_device(cast_device&& other);
    ~cast_device();

    cast_device(const mdns::mdns_res& res, const char* ssl_cert, const char* ssl_key);
    
    bool connect();

    bool disonnect();

    bool launch_app(std::string_view app_id);

private:

    bool send(const std::string_view nspace, const std::string_view dest_id, std::string_view payload) const;

    bool send_json(const std::string_view nspace, const std::string_view dest_id, json payload) const
    {
        return send(nspace, dest_id, payload.dump());
    }

    bool receive(CastMessage& dest_msg) const;

    bool receive_payload(json& dest_payload) const;



    std::unique_ptr<socketwrapper::SSLTCPSocket> m_sock_ptr;

    std::atomic<bool> m_connected = ATOMIC_VAR_INIT(false);

    std::future<void> m_heartbeat;

    std::string m_name;                           // From PTR record

    std::string m_target;                         // From SRV record

    uint32_t m_port;                              // From SRV record

    std::string m_ip;                             // From A record

    std::map<std::string, std::string> m_txt;     // From TXT record

    uint64_t m_request_id = 0;
    
};

} // namespace googlecast

#endif
