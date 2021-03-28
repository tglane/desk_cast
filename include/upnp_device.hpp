#ifndef DLNA_DEVICE_HPP
#define DLNA_DEVICE_HPP

#include <string>
#include <vector>

#include "ssdp_discovery.hpp"
#include "socketwrapper.hpp"

namespace upnp
{

struct upnp_service
{
    std::string id;
    std::string control_url;
    std::string scpd_url;
    std::string event_sub_url;
};

class upnp_media_renderer
{
public:
    upnp_media_renderer() = delete;
    upnp_media_renderer(const upnp_media_renderer&) = delete;
    upnp_media_renderer& operator=(const upnp_media_renderer&) = delete;
    upnp_media_renderer(upnp_media_renderer&&) = default;
    upnp_media_renderer& operator=(upnp_media_renderer&&) =default;
    ~upnp_media_renderer() = default;

    upnp_media_renderer(const discovery::ssdp_res& res);

    bool connect();

    bool service_available(std::string_view service_id) const;

    // TODO
    bool use_service(std::string_view service_id) const;

    const upnp_service& get_service_information(std::string_view service_id) const;

private:

    std::unique_ptr<net::tcp_connection<net::ip_version::v4>> m_sock {nullptr};

    discovery::ssdp_res m_discovery_res;

    std::vector<upnp_service> m_services;

    bool m_connected = false;

};

} // namespace upnp

#endif

