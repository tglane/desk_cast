#ifndef DLNA_DEVICE_HPP
#define DLNA_DEVICE_HPP

#include <string>
#include <vector>

#include "ssdp_discovery.hpp"
#include "socketwrapper.hpp"

namespace dlna
{

struct dlna_service
{
    std::string id;
    std::string control_url;
    std::string scpd_url;
    std::string event_sub_url;
};

class dlna_media_renderer
{
public:
    dlna_media_renderer() = delete;
    dlna_media_renderer(const dlna_media_renderer&) = delete;
    dlna_media_renderer& operator=(const dlna_media_renderer&) = delete;
    dlna_media_renderer(dlna_media_renderer&&) = default;
    dlna_media_renderer& operator=(dlna_media_renderer&&) =default;
    ~dlna_media_renderer() = default;

    dlna_media_renderer(const discovery::ssdp_res res);

    bool connect();

   const dlna_service* get_service_information(const std::string& service_id) const;

private:

    std::unique_ptr<net::tcp_connection<net::ip_version::v4>> m_sock {nullptr};

    discovery::ssdp_res m_discovery_res;

    std::vector<dlna_service> m_services;

    bool m_connected = false;

};

} // namespace upnp

#endif

