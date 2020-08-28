#ifndef MDNS_DISCOVERY_HPP
#define MDNS_DISCOVERY_HPP

#include <string>
#include <string_view>
#include <vector>
#include <map>

#define QUERY_IP "224.0.0.251"
#define QUERY_PORT 5353
#define QUERY_TIME 5000

namespace mdns
{

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct r_data
{
    unsigned short type;
    unsigned short _class;
    unsigned int ttl;
    unsigned short data_len;
};
#pragma pack(pop)
 
//Pointers to resource record contents
struct res_record
{
    unsigned char* name;
    r_data* resource;
    unsigned char* rdata;
};
 
std::vector<std::string> mdns_query(const std::string& record_name);

}

#endif
