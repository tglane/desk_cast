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

std::string parse_ptr_record(const mdns_record& rec);

std::map<std::string, std::string> parse_txt_record(const mdns_record& rec);

void parse_srv_record(const mdns_record& rec);

sockaddr_storage parse_a_record(const mdns_record& rec);

}

#endif
