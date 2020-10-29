#ifndef CAST_DEVICE_HPP
#define CAST_DEVICE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <map>
#include <deque>
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

class receiver;

class cast_device
{
public:

    cast_device() = delete;
    cast_device(const cast_device&) = delete;
    cast_device& operator=(const cast_device&) = delete;
    cast_device(cast_device&& other);
    cast_device& operator=(cast_device&& other) = default;
    ~cast_device();

    cast_device(const discovery::mdns_res& res, const char* ssl_cert, const char* ssl_key);
    
    bool connect();

    bool disonnect();

    bool app_available(std::string_view app_id) const;

    cast_app& launch_app(const std::string& app_id);

    void close_app();

    /// TODO
    bool volume_up();
    bool volume_down();
    bool toggle_mute();
    ///

    json get_status() const;

    inline cast_app& get_app() const
    {
        return *m_active_app;
    }

    inline const std::string& get_name() const
    {
        return m_name;
    }

private:

    bool send(const std::string_view nspace, std::string_view payload, const std::string_view dest_id = "receiver-0") const;

    bool send_json(const std::string_view nspace, json payload, const std::string_view dest_id = "receiver-0") const
    {
        return send(nspace, payload.dump(), dest_id);
    }

    json read(uint32_t request_id) const;


    std::future<void> m_heartbeat;                  // Future to store async function sending a heartbeat signal

    std::unique_ptr<receiver> m_receiver;           // Utility class to wait for responses of the connected cast device

    std::unique_ptr<cast_app> m_active_app;

    std::shared_ptr<socketwrapper::SSLTCPSocket> m_sock_ptr;

    std::atomic<bool> m_connected = ATOMIC_VAR_INIT(false);

    std::string m_name;                             // From PTR record

    std::string m_target;                           // From SRV record

    uint32_t m_port;                                // From SRV record

    std::string m_ip;                               // From A record

    std::map<std::string, std::string> m_txt;       // From TXT record

    mutable uint64_t m_request_id = 0;              // Up counting id to identify requests and responses

    friend class cast_app;
};

} // namespace googlecast

#endif
