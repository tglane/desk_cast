#include "dlna_device.hpp"

#include "http/request.hpp"
#include "http/response.hpp"

namespace dlna
{

dlna_media_renderer::dlna_media_renderer(discovery::ssdp_res res)
    : m_sock {AF_INET}, m_discovery_res {res}
{
}

bool dlna_media_renderer::connect()
{
    try {
        m_sock.connect(m_discovery_res.location.port, m_discovery_res.location.ip);
        m_connected = true;
        return true;
    } catch(socketwrapper::SocketConnectingException&) {
        return false;
    }
}

void dlna_media_renderer::get_app_information() const
{
    if(!m_connected)
        return;

    http::response res {m_sock.read_vector<char>(4096).data()};
}

}
