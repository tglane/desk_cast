#ifndef HTTP_WEBSERVER_HPP
#define HTTP_WEBSERVER_HPP

#include "socketwrapper.hpp"

#include "socketwrapper.hpp"

namespace http
{

// TODO Currently without ssl ... need to fix that
class webserver
{
public:
    webserver() = delete;
    webserver(const webserver&) = delete;
    webserver& operator=(const webserver&) = delete;
    webserver(webserver&&) = default;
    webserver& operator=(webserver&&) = default;
    ~webserver() = default;

    webserver(uint16_t port, const char* cert_path, const char* key_path)
        : m_acceptor {"0.0.0.0", port}
    {}

    void serve(std::atomic<bool>& run_condition);

private:

    // socketwrapper::SSLTCPSocket m_sock;
    net::tcp_acceptor<net::ip_version::v4> m_acceptor;

};

} // namespace http

#endif
