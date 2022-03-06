#ifndef CAST_DEVICE_HPP
#define CAST_DEVICE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <future>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "../protos/cast_channel.pb.h"
#include "device.hpp"
#include "socketwrapper.hpp"
#include "json.hpp"
#include "mdns_discovery.hpp"

using nlohmann::json;
using cast_message = cast_channel::CastMessage;
using cond_ptr = std::unique_ptr<std::condition_variable>;

namespace googlecast
{

struct app_details
{
    std::string id;
    std::string session_id;
    std::string transport_id;
    json namespaces;

    explicit operator bool() const
    {
        return (!id.empty() && !transport_id.empty() && !session_id.empty());
    }

    void clear()
    {
        id.clear();
        session_id.clear();
        transport_id.clear();
        namespaces.clear();
    }
};

class cast_device : public device
{
public:

    cast_device() = delete;
    cast_device(const cast_device&) = delete;
    cast_device& operator=(const cast_device&) = delete;
    cast_device(cast_device&& other) noexcept;
    cast_device& operator=(cast_device&& other) noexcept;
    ~cast_device() override;

    cast_device(const discovery::mdns_res& res, std::string_view ssl_cert, std::string_view ssl_key);

    bool connect() override;

    bool disconnect() override;

    bool app_available(std::string_view app_id) const;

    bool launch_app(std::string_view app_id, json&& launch_payload);

    void close_app();

    bool set_volume(double level) override;

    bool set_muted(bool muted) override;

    json get_status() const;

    bool connected() const
    {
        return m_connected.load();
    }

    inline app_details get_app_details() const
    {
        return m_active_app;
    }

    inline const std::string& get_name() const override
    {
        return m_name;
    }

private:

    struct ssl_keypair_path
    {
        std::string cert_path;
        std::string key_path;
    };

    class device_connection;

    /// Private member functions

    bool send(std::string_view nspace, std::string_view payload, std::string_view dest_id = "receiver-0") const;

    bool send_json(const std::string_view nspace, const json& payload, const std::string_view dest_id = "receiver-0") const
    {
        return send(nspace, std::string_view {payload.dump()}, dest_id);
    }

    json send_recv(std::string_view nspace, const json& payload, std::string_view dest_id = "receiver-0") const;

    /// Private member variables

    ssl_keypair_path m_keypair;

    std::unique_ptr<device_connection> m_connection {nullptr};

    mutable std::unordered_map<uint64_t, std::pair<cond_ptr, json>> m_msg_store;

    mutable std::mutex m_msg_mutex;

    app_details m_active_app;

    std::atomic<bool> m_connected = ATOMIC_VAR_INIT(false);

    std::string m_name;                                 // From PTR record

    std::string m_target;                               // From SRV record

    std::unordered_map<std::string, std::string> m_txt; // From TXT record

    std::string m_ip;                                   // From A record

    uint32_t m_port;                                    // From SRV record

    mutable uint64_t m_request_id = 0;                  // Up counting id to identify requests and responses

};

} // namespace googlecast

#endif
