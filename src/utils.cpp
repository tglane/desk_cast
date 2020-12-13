#include <utils.hpp>

#include <array>
#include <bitset>
#include <stdexcept>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>

namespace utils
{

const char* error_msg = "Unable to get local ip address";

std::string get_local_ipaddr()
{
    ifaddrs* addrs;
    if (getifaddrs(&addrs))
        throw std::runtime_error {error_msg};

    for (ifaddrs* curr_addr = addrs; curr_addr != nullptr; curr_addr = curr_addr->ifa_next)
    {
        if(curr_addr->ifa_addr == nullptr)
            continue;

        int family = curr_addr->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6)
        {
            std::array<char, NI_MAXHOST> host;

            int s = getnameinfo(curr_addr->ifa_addr, (family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6),
                host.data(), NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if(s != 0)
                throw std::runtime_error {error_msg};

            std::bitset<sizeof(unsigned int) * 8> flags {curr_addr->ifa_flags};
            if (flags.test(IFF_UP) && !flags.test(IFF_LOOPBACK))
                return host.data();
        }
    }

    freeifaddrs(addrs);
    throw std::runtime_error {error_msg};
}

} // utils
