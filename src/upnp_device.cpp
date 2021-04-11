#include "upnp_device.hpp"

#include "http/request.hpp"
#include "http/response.hpp"

#include "fmt/format.h"

#include <chrono>
#include <algorithm>
#include <thread>

using namespace std::chrono_literals;
using namespace rapidxml;

namespace upnp
{

upnp_device::upnp_device(const discovery::ssdp_res& res)
    : m_addr {res.location.ip},
      m_port {res.location.port},
      m_description_path {res.location.path},
      m_name {res.usn} // TODO
{}

upnp_device::upnp_device(discovery::ssdp_res&& res)
    : m_addr {static_cast<std::string&&>(res.location.ip)},
      m_port {res.location.port},
      m_description_path {static_cast<std::string&&>(res.location.path)},
      m_name {static_cast<std::string&&>(res.usn)} // TODO
{}

bool upnp_device::connect()
{
    net::tcp_connection<net::ip_version::v4> sock {m_addr, m_port};

    std::string req_str = fmt::format("GET {} HTTP/1.1\r\nHOST: {}:{}\r\n\r\n", m_description_path, m_addr, m_port);
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
        // The serviceId ist the last part of a schema string with the form urn:upnp-org:serviceId:RenderingControl
        std::string_view service_view {service_node->first_node("serviceId")->value()};
        size_t service_offset = service_view.find_last_of(":") + 1;

        const auto& serv = m_services.emplace_back(upnp_service {
            service_node->first_node("serviceId")->value() + service_offset,
            service_node->first_node("controlURL")->value(),
            service_node->first_node("SCPDURL")->value(),
            service_node->first_node("eventSubURL")->value()
        });
    }

    m_connected = true;
    return true;
}

bool upnp_device::disconnect()
{
    // Currently just a dummy function because there is no real standing connection between a control point and a device
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
        "POST {0} HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction: \"urn:schemas-upnp-org:service:{1}:1#{2}\"\r\n\r\n<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:{2} xmlns:u=\"urn:schemas-upnp-org:service:{1}:1\"><InstanceID>0</InstanceID>{3}</u:{2}></s:Body></s:Envelope>\r\n\r\n",
        opt_service->get().control_url,
        service_id,
        param.action,
        param.body // Contains current uri and uri metadata when using av transport for example
    );

    fmt::print("Request: {}", request);
    try {
        net::tcp_connection<net::ip_version::v4> sock {m_addr, m_port};
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

    net::tcp_connection<net::ip_version::v4> sock {m_addr, m_port};
    sock.send(net::span {request.begin(), request.end()});

    net::tcp_connection<net::ip_version::v4> sock_two {m_addr, m_port};
    std::string play_req {"POST /dmr/control_2 HTTP/1.1\r\nContent-Type: text/xml\r\nSOAPAction: \"urn:schemas-upnp-org:service:AVTransport:1#Play\"\r\n\r\n<?xml version=\"1.0\" encoding=\"utf-8\"?><s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\"><InstanceID>0</InstanceID><Speed>1</Speed></u:Play></s:Body></s:Envelope>\r\n\r\n"};
    sock_two.send(net::span {play_req.begin(), play_req.end()});
}

bool upnp_device::set_volume(double level)
{
    // TODO Implement
    return true;
}

bool upnp_device::set_muted(bool muted)
{
    // TODO Implement
    return true;
}

} // namespace dlna
