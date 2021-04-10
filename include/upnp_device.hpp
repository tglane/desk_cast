#ifndef DLNA_DEVICE_HPP
#define DLNA_DEVICE_HPP

#include <string>
#include <vector>
#include <optional>

#include "ssdp_discovery.hpp"
#include "socketwrapper.hpp"
#include "rapidxml/rapidxml_ext.hpp"

namespace upnp
{

struct upnp_service
{
    std::string id;
    std::string control_url;
    std::string scpd_url;
    std::string event_sub_url;
};

struct service_parameter
{
    // TODO
    std::string action;
    std::string body;
};

class upnp_device
{
// TODO Add device status enum class?
public:

    upnp_device() = delete;
    upnp_device(const upnp_device&) = delete;
    upnp_device& operator=(const upnp_device&) = delete;
    upnp_device(upnp_device&&) = default;
    upnp_device& operator=(upnp_device&&) = default;
    ~upnp_device() = default;

    explicit upnp_device(const discovery::ssdp_res& res);

    explicit upnp_device(discovery::ssdp_res&& res);

    bool connect();

    bool service_available(std::string_view service_id) const;

    std::optional<std::reference_wrapper<const upnp_service>> get_service_information(std::string_view service_id) const;

    bool use_service(std::string_view service_id, const service_parameter& param) const;

    void launch_media() const; // TODO Remove when finished testing

    bool connected() const
    {
        return m_connected;
    }

private:

    bool m_connected = false;

    std::string m_addr;

    uint16_t m_port;

    std::string m_description_path;

    std::vector<upnp_service> m_services;

};

} // namespace upnp

#endif
