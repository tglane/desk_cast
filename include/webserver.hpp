#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <socketwrapper/SSLTCPSocket.hpp>
#include <socketwrapper/TCPSocket.hpp>

// TODO Currently without ssl ... need to fix that
class ssl_webserver
{
public:

    ssl_webserver(int32_t port, const char* cert_path, const char* key_path);

    void serve(std::atomic<bool>& run_condition);

private:

    socketwrapper::TCPSocket m_sock;

};

#endif
