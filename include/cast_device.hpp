#ifndef CAST_DEVICE_HPP
#define CAST_DEVICE_HPP

#include <string>
#include <map>

#include <socketwrapper/SSLTCPSocket.hpp>

struct cast_device
{
    // socketwrapper::SSLTCPSocket m_sock;

    std::string name;       // From PTR record

    std::string target;     // From SRV record

    uint32_t port;        // From SRV record

    std::string ip;         // From A record

    std::map<std::string, std::string> txt;     // From txt record

};

#endif
