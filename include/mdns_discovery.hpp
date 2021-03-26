#ifndef MDNS_DISCOVERY_HPP
#define MDNS_DISCOVERY_HPP

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include <sys/socket.h>

#include "socketwrapper.hpp"

#define QUERY_IP "224.0.0.251"
#define QUERY_PORT 5353
#define QUERY_TIME 5000

namespace discovery
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
    net::connection_tuple peer;
    uint16_t qtype;
    std::string qname;
    std::vector<mdns_record> records;
};

std::vector<mdns_res> mdns_discovery(const std::string& record_name);

void parse_ptr_record(const mdns_record& rec, std::string& dest_name);

void parse_txt_record(const mdns_record& rec, std::unordered_map<std::string, std::string>& dest_txt);

void parse_srv_record(const mdns_record& rec, uint32_t& dest_port, std::string& dest_target);

void parse_a_record(const mdns_record& rec, std::string& dest_addr);

} // namespace discovery

#endif
