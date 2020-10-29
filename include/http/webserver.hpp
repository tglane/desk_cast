#ifndef HTTP_WEBSERVER_HPP
#define HTTP_WEBSERVER_HPP

#include <socketwrapper/SSLTCPSocket.hpp>
#include <socketwrapper/TCPSocket.hpp>

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

    webserver(int32_t port, const char* cert_path, const char* key_path);

    void serve(std::atomic<bool>& run_condition);

private:

    socketwrapper::TCPSocket m_sock;

};

} // namespace http

#endif
