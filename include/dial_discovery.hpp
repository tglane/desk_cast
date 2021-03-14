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

using ssdp_res = std::map<std::string, std::string>;

ssdp_res parse_request(std::string_view view);

void parse_location(const std::string& location, std::string& addr, 
    std::string& path, uint16_t& port);

std::vector<ssdp_res> upnp_discovery();

} // namespace discovery

#endif
