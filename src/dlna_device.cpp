#include "dlna_device.hpp"

#include "http/request.hpp"
#include "http/response.hpp"
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include <chrono>
#include <algorithm>

using namespace std::chrono_literals;

namespace dlna
{

dlna_media_renderer::dlna_media_renderer(discovery::ssdp_res res)
    : m_discovery_res {res}
{}

bool dlna_media_renderer::connect()
{
    try {
        m_sock = std::make_unique<net::tcp_connection<net::ip_version::v4>>(m_discovery_res.location.ip, m_discovery_res.location.port);
        m_connected = true;
    } catch(std::runtime_error&) {
        return false;
    }

    const discovery::ssdp_location& loc = m_discovery_res.location;
    std::string req_str = ("GET " + loc.path + " HTTP/1.1\r\nHOST: " + loc.ip + ":" + std::to_string(loc.port) +  "\r\n\r\n");
    m_sock->send(req_str);

    // while(!m_sock.bytes_available())
    //     std::this_thread::sleep_for(100ms);
    std::vector<char> buffer = m_sock->read<char>(4096);

    std::string_view buffer_view {buffer.data(), buffer.size()};
    std::cout << "Size: " << buffer.size() << std::endl;
    std::cout << buffer_view << std::endl;
    // http::response {m_sock.read_vector<char>(4096)};

    rapidxml::xml_document<char> doc;
    // TODO ignore http headers .. just pass in the body here .. maybe rewrite http::response class?
    doc.parse<0>(buffer.data());

    // std::cout << doc;
    std::cout << doc.first_node()->name() << std::endl;

    return true;
}

const dlna_service* dlna_media_renderer::get_service_information(const std::string& service_id) const
{
    auto it = std::find_if(m_services.begin(), m_services.end(), [&s_id = service_id](const dlna_service& service) {
        return service.id == s_id;
    });

    return (it != m_services.end()) ? &(*it) : nullptr;
}

} // namespace dlna

