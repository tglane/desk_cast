#ifndef DLNA_DEVICE_HPP
#define DLNA_DEVICE_HPP

#include <string>
#include <vector>

#include "ssdp_discovery.hpp"
#include "socketwrapper/TCPSocket.hpp"

namespace dlna
{

struct dlna_service
{

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

    void get_app_information() const;

private:

    socketwrapper::TCPSocket m_sock;

    discovery::ssdp_res m_discovery_res;

    std::vector<dlna_service> m_services;

    bool m_connected = false;

};

} // namespace upnp

#endif
