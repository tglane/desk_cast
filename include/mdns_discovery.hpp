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

enum mdns_rec_type : uint16_t {
    A = 1,
    AAAA = 28,
    CNAME = 5,
    PTR = 12,
    SRV = 33,
    TXT = 16
};

struct mdns_record
{
    std::string name;
    uint16_t type;
    size_t length;
    std::vector<char> data;
};

struct mdns_res
{
    sockaddr_storage peer;
    uint16_t qtype;
    std::string qname;
    std::vector<mdns_record> records;
};

std::vector<mdns_res> mdns_discovery(const std::string& record_name);

}

#endif
