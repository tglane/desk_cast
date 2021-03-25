#include "dlna_device.hpp"

#include "http/request.hpp"
#include "http/response.hpp"
#include "rapidxml/rapidxml_ext.hpp"

#include <chrono>
#include <algorithm>
#include <thread>
#include <sys/ioctl.h>

using namespace std::chrono_literals;
using namespace rapidxml;

namespace dlna
{

dlna_media_renderer::dlna_media_renderer(discovery::ssdp_res res)
    : m_discovery_res {res}
{}

bool dlna_media_renderer::connect()
{
    try {
        m_sock = std::make_unique<net::tcp_connection<net::ip_version::v4>>(m_discovery_res.location.ip, m_discovery_res.location.port);
    } catch(std::runtime_error&) {
        return false;
    }

    const discovery::ssdp_location& loc = m_discovery_res.location;
    std::string req_str = ("GET " + loc.path + " HTTP/1.1\r\nHOST: " + loc.ip + ":" + std::to_string(loc.port) +  "\r\n\r\n");
    m_sock->send(net::span {req_str.begin(), req_str.end()});

    // Receive HTTP response containing XML body
    std::array<char, 4096> buffer;
    size_t bytes_read = 0;
    for(size_t br = 0; bytes_read < 4096; )
    {
        br = m_sock->read(net::span {buffer.data() + br, buffer.size() - br});
        if(br == 0)
            break;
        else
            bytes_read += br;
    }

    std::string_view header_termination {"\r\n\r\n"};
    auto xml_start = std::search(buffer.begin(), buffer.end(), header_termination.begin(), header_termination.end());
    if(xml_start == buffer.end())
        return false;
    xml_start += 4;

    xml_document<char> doc;
    try {
        doc.parse<0>(&(*xml_start));
    } catch(rapidxml::parse_error&) {
        // Catch wrongly received data
        return false;
    }

    xml_node<char>* device_node = doc.first_node()->first_node("device");
    xml_node<char>* service_root = device_node->first_node("serviceList");
    for(xml_node<char>* service_node = service_root->first_node("service"); service_node; service_node = service_node->next_sibling())
    {
        m_services.emplace_back(dlna_service {
            service_node->first_node("serviceId")->value(),
            service_node->first_node("controlURL")->value(),
            service_node->first_node("SCPDURL")->value(),
            service_node->first_node("eventSubURL")->value()
        });
    }

    m_connected = true;
    return true;
}

const dlna_service* dlna_media_renderer::get_service_information(const std::string& service_id) const
{
    // DLNA devices typically have around 3 to 4 services
    // So using a vector and search for it should be as efficient as using unordered_map/set if not more efficient
    auto it = std::find_if(m_services.begin(), m_services.end(), [&s_id = service_id](const dlna_service& service) {
        return service.id == s_id;
    });

    return (it != m_services.end()) ? &(*it) : nullptr;
}

} // namespace dlna

