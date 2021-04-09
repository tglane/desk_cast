#include "upnp_device.hpp"

#include "http/request.hpp"
#include "http/response.hpp"

#include "fmt/format.h"

#include <chrono>
#include <algorithm>
#include <thread>
#include <sys/ioctl.h>

using namespace std::chrono_literals;
using namespace rapidxml;

namespace upnp
{

upnp_device::upnp_device(const discovery::ssdp_res& res)
    : m_discovery_res {res}
{}

bool upnp_device::connect()
{
    net::tcp_connection<net::ip_version::v4> sock {m_discovery_res.location.ip, m_discovery_res.location.port};

    const discovery::ssdp_location& loc = m_discovery_res.location;
    std::string req_str = ("GET " + loc.path + " HTTP/1.1\r\nHOST: " + loc.ip + ":" + std::to_string(loc.port) +  "\r\n\r\n");
    sock.send(net::span {req_str.begin(), req_str.end()});

    // Receive HTTP response containing XML body
    std::array<char, 4096> buffer;
    size_t bytes_read = 0;
    for(size_t br = 0; bytes_read < 4096; )
    {
        br = sock.read(net::span {buffer.data() + br, buffer.size() - br});
        if(br == 0)
            break;
        else
            bytes_read += br;
    }

    const std::string_view header_termination {"\r\n\r\n"};
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

    // TODO Store device details in member variable
    xml_node<char>* device_node = doc.first_node()->first_node("device");
    xml_node<char>* service_root = device_node->first_node("serviceList");
    for(xml_node<char>* service_node = service_root->first_node("service"); service_node; service_node = service_node->next_sibling())
    {
        const auto& serv = m_services.emplace_back(upnp_service {
            service_node->first_node("serviceId")->value(),
            service_node->first_node("controlURL")->value(),
            service_node->first_node("SCPDURL")->value(),
            service_node->first_node("eventSubURL")->value()
        });

        fmt::print("ID: {} | Service url: {}\n", serv.id, serv.control_url);
    }

    m_connected = true;
    return true;
}

bool upnp_device::service_available(std::string_view service_id) const
{
    // DLNA devices typically have around 3 to 4 services
    // So using a vector and search for it should be as efficient as using unordered_map/set if not more efficient
    if(std::find_if(m_services.begin(), m_services.end(), [&s_id = service_id](const upnp_service& service) {
        return service.id == s_id;
    }) != m_services.end())
        return true;
    else
        return false;
}

std::optional<std::reference_wrapper<const upnp_service>> upnp_device::get_service_information(std::string_view service_id) const
{
    // DLNA devices typically have around 3 to 4 services
    // So using a vector and search for it should be as efficient as using unordered_map/set if not more efficient
    auto it = std::find_if(m_services.begin(), m_services.end(), [&s_id = service_id](const upnp_service& service) {
        return service.id == s_id;
    });

    if(it != m_services.end())
        return *it;
    else
        return std::nullopt;
}

bool upnp_device::use_service(std::string_view service_id, const service_parameter& param) const
{
    auto opt_service = get_service_information(service_id);
    if(!opt_service)
        return false;

    // TODO Get format parameter from service_parameter
    std::string request = fmt::format(
        "POST {0} HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction: \"{1}:1#{2}\"\r\n\r\n<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:{2} xmlns:u=\"{1}:1\"><InstanceID>0</InstanceID>{3}</u:{1}></s:Body></s:Envelope>\r\n\r\n",
        opt_service->get().control_url,
        service_id,
        param.method,
        param.method_body // Contains current uri and uri metadata when using av transport for example
    );

    try {
        net::tcp_connection<net::ip_version::v4> sock {m_discovery_res.location.ip, m_discovery_res.location.port};
        sock.send(net::span {request});
        return true;
    } catch(std::runtime_error&) {
        return false;
    }
}

void upnp_device::launch_media() const
{
    // Method just for checking how to instruct a upnp media renderer to stream a (video) file
    // Later on there will be only the function use_service() in this class and maybe a seperate upnp renderer/streamer class
    // std::string request {"POST /dmr/control_2 HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction: \"urn:schemas-upnp-org:service:AVTransport:1#SetAVTransportURI\"\r\n\r\n<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID><CurrentURI>http://techslides.com/demos/sample-videos/small.mp4</CurrentURI><CurrentURIMetaData /></u:SetAVTransportURI></s:Body></s:Envelope>\r\n\r\n"};


    std::string request {"POST /dmr/control_2 HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction: \"urn:schemas-upnp-org:service:AVTransport:1#SetAVTransportURI\"\r\n\r\n<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID><CurrentURI>http://192.168.178.20:5770/test_video.mp4</CurrentURI><CurrentURIMetaData /></u:SetAVTransportURI></s:Body></s:Envelope>\r\n\r\n"};

    // std::string request {"POST /dmr/control_2 HTTP/1.1\r\nContent-Type: text/xml; charset=utf-8\r\nSOAPAction: \"urn:schemas-upnp-org:service:AVTransport:1#SetAVTransportURI\"\r\n\r\n<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID><CurrentURI>http://192.168.178.20:5770/index.m3u8</CurrentURI><CurrentURIMetaData /></u:SetAVTransportURI></s:Body></s:Envelope>\r\n\r\n"};

    net::tcp_connection<net::ip_version::v4> sock {m_discovery_res.location.ip, m_discovery_res.location.port};
    sock.send(net::span {request.begin(), request.end()});

    net::tcp_connection<net::ip_version::v4> sock_two {m_discovery_res.location.ip, m_discovery_res.location.port};
    std::string play_req {"POST /dmr/control_2 HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction: \"urn:schemas-upnp-org:service:AVTransport:1#Play\"\r\n\r\n<?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID><Speed>1</Speed></u:Play></s:Body></s:Envelope>\r\n\r\n"};
    sock_two.send(net::span {play_req.begin(), play_req.end()});
}

} // namespace dlna
