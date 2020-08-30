#ifndef MDNS_DISCOVERY_HPP
#define MDNS_DISCOVERY_HPP

#include <string>
#include <string_view>
#include <vector>
#include <map>

#include <sys/socket.h>

#define QUERY_IP "224.0.0.251"
#define QUERY_PORT 5353
#define QUERY_TIME 5000

namespace mdns
{

struct mdns_record
{
    uint16_t type;
    size_t pos;
    size_t len;
    std::string name;
};

struct mdns_res
{
    sockaddr_storage peer;
    std::string address;
    int port;
    uint16_t qtype;
    std::string qname;
    std::vector<mdns_record> records;
};

std::vector<mdns_res> mdns_discovery(const std::string& record_name);

}

#endif
