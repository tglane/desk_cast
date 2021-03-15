#ifndef DIAL_DISCOVERY_HPP
#define DIAL_DISCOVERY_HPP

#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace discovery
{

#define DISCOVERY_IP "239.255.255.250"
#define DISCOVERY_PORT 1900
#define DISCOVERY_TIME 5000

// using ssdp_res = std::map<std::string, std::string>;

struct ssdp_location
{
    std::string ip;
    std::string path;
    uint16_t port;
};

struct ssdp_res
{
    ssdp_location location;
    std::string cache_control;
    std::string ext;
    std::string server;
    std::string usn;
    std::string st;
    std::string wakeup;
};

std::vector<ssdp_res> ssdp(const std::string& service_type);

} // namespace discovery

#endif
