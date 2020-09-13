#ifndef CHROMECAST_HPP
#define CHROMECAST_HPP

#include <socketwrapper/SSLTCPSocket.hpp>

class chromecast
{
public:

    void set_addr(const sockaddr_storage& peer)
    {
        m_peer = peer;
    }

private:

    socketwrapper::SSLTCPSocket m_sock;

    sockaddr_storage m_peer;

    uint32_t m_port;
    std::string ip;

};

#endif
