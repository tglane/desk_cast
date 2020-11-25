#include <mdns_discovery.hpp>

#include <socketwrapper/UDPSocket.hpp>

#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <streambuf>
#include <istream>
#include <sstream>
#include <stdexcept>

#include <iostream>

namespace discovery
{

constexpr uint16_t MDNS_RESPONSE_FLAG = 0x8400;
constexpr uint8_t MDNS_OFFSET_TOKEN = 0xC0;

//DNS header structure
struct dns_header
{
    uint16_t id; // identification number
 
    unsigned char rd :1; // recursion desired
    unsigned char tc :1; // truncated message
    unsigned char aa :1; // authoritive answer
    unsigned char opcode :4; // purpose of message
    unsigned char qr :1; // query/response flag
 
    unsigned char rcode :4; // response code
    unsigned char cd :1; // checking disabled
    unsigned char ad :1; // authenticated data
    unsigned char z :1; // its z! reserved
    unsigned char ra :1; // recursion available
 
    uint16_t q_count; // number of question entries
    uint16_t ans_count; // number of answer entries
    uint16_t auth_count; // number of authority entries
    uint16_t add_count; // number of resource entries
};
 
//Constant sized fields of query structure
struct dns_question
{
    uint16_t qtype;
    uint16_t qclass;
};

//Structure of a Query
struct dns_query
{
    unsigned char *name;
    dns_question *ques;
};

static void to_dns_name_format(char* dns, std::string_view host)
{
    int lock = 0;
     
    for(int i = 0; i <= host.size(); i++)
    {
        if(i == host.size() || host[i] == '.')
        {
            *dns++ = i - lock;
            while(lock < i)
            {
                *dns++=host[lock];
                lock++;
            }
            lock++;
        }
    }
    *dns++ ='\0';
}

static size_t read_fqdn(const std::vector<char>& data, size_t offset, std::string& result)
{
    // fqdn = fully qualified domain names
    result.clear();
    size_t pos = offset;

    while(1)
    {
        if(pos >= data.size())
            return 0;

        uint8_t len = data[pos++];
        if(pos + len > data.size())
            return 0;
        if(len == 0)
            break;

        if(!result.empty())
            result.append(".");
        result.append(reinterpret_cast<const char*>(&data[pos]), len);
        pos += len;
    }
    return pos - offset;
}

static mdns_res parse_mdns_answer(std::vector<char>& buffer)
{
    dns_header* dns = reinterpret_cast<dns_header*>(buffer.data());
    
    if(buffer.empty() || ntohs(dns->ans_count) == 0 || ntohs(dns->q_count) > 0)
        throw std::invalid_argument {"Not a valid mdns response."};
    
    mdns_res result;

    std::istringstream b_stream {std::string(reinterpret_cast<char*>(buffer.data()), buffer.size())};
    b_stream.exceptions(std::istream::failbit | std::istream::badbit | std::istream::eofbit);

    try {
        b_stream.ignore(sizeof(dns_header));

        // Parse records
        for(size_t i = 0; i < (ntohs(dns->ans_count) + ntohs(dns->auth_count) + ntohs(dns->add_count)); i++)
        {
            uint16_t u16;
            mdns_record& rec = result.records.emplace_back();

            // Check if name is empty (?)
            size_t n_len;
            if(static_cast<uint8_t>(buffer[b_stream.tellg()]) == MDNS_OFFSET_TOKEN ||
                static_cast<uint8_t>(buffer[b_stream.tellg()]) == 0xC1) // TODO check what this means ... got this from wireshark (A record) but no idea what it is
                n_len = 2;
            else
                n_len = read_fqdn(buffer, b_stream.tellg(), rec.name);
            
            b_stream.ignore(n_len);
            b_stream.read(reinterpret_cast<char*>(&u16), 2);
            rec.type = ntohs(u16);

            b_stream.ignore(6);
            b_stream.read(reinterpret_cast<char*>(&u16), 2);
            rec.length = ntohs(u16);

            rec.data.resize(rec.length);
            b_stream.read(rec.data.data(), rec.length);

        }
    } catch(std::exception& e) {
        // Response could not be parsed like an expected response from a chromecast
        return {};
    }

    return result;
}

std::vector<mdns_res> mdns_discovery(const std::string& record_name)
{
    bool stop = false;
    socketwrapper::UDPSocket q_sock {AF_INET};
    std::vector<mdns_res> res;

    // Set up mdns query
    char query[6536] = { 0 };
    dns_header* dns = reinterpret_cast<dns_header*>(&query);
    dns->rd = 1;
    dns->q_count = htons(1);

    char* qname = reinterpret_cast<char*>(&query[sizeof(dns_header)]);
    to_dns_name_format(qname, record_name.data());

    dns_question* qinfo = reinterpret_cast<dns_question*>(&query[sizeof(dns_header) + (strlen((const char*) qname) + 1)]);
    qinfo->qtype = htons(12);
    qinfo->qclass = htons(1);

    // Send up query
    q_sock.bind("0.0.0.0", 5353);
    q_sock.send_to<char>(query, sizeof(dns_header) + (strlen((const char*)qname)+1) + sizeof(dns_question), QUERY_PORT, QUERY_IP);

    // Receive answers for QUERY_TIME seconds
    auto fut = std::async(std::launch::async, [&stop, &q_sock, &res]()
    {
        while(!stop)
        {
            if(q_sock.bytes_available())
            {
                sockaddr_storage peer;
                std::vector<char> buffer = q_sock.receive_vector<char>(4096, reinterpret_cast<sockaddr_in*>(&peer));

                try {
                    mdns_res tmp = parse_mdns_answer(buffer);
                    tmp.peer = peer;

                    res.push_back(std::move(tmp));
                } catch(std::exception& e) {
                    // ... No valid mdns answer so just skip it
                }
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_TIME));
    stop = true;
    fut.get();

    q_sock.close();
    return res;
}

void parse_ptr_record(const mdns_record& rec, std::string& dest_name)
{
    // Check if name in record contains pointer
    if(static_cast<uint8_t>(*(rec.data.end() - 2)) == MDNS_OFFSET_TOKEN)
        dest_name = std::string {rec.data.begin() + 1, rec.data.end() - 2};
    else
        dest_name = std::string {rec.data.begin() + 1, rec.data.end()};
}

void parse_txt_record(const mdns_record& rec, std::map<std::string, std::string>& dest_txt)
{
    // Structure of rec.data: 
    // [1 byte -> len][key=value of length len]...
    size_t off = 0;
    while(off < rec.data.size())
    {
        size_t len = static_cast<uint8_t>(rec.data[off++]);

        std::string_view view {&rec.data[off], len};
        size_t sep = view.find_first_of('=');
        if(sep != std::string::npos)
            dest_txt.insert(std::pair<std::string, std::string> {
                std::string {&rec.data[off], sep},
                std::string {&rec.data[off + sep + 1], len - sep}});

        off += len;
    }
}

void parse_srv_record(const mdns_record& rec, uint32_t& dest_port, std::string& dest_target)
{
    // port : byte 4 and 5
    // target : byte 6 to size()
    dest_port = ntohs(*reinterpret_cast<const uint32_t*>(&rec.data[4]));
    if(rec.data.size() > 7)
        dest_target = std::string {rec.data.begin() + 7, rec.data.end()};
}

void parse_a_record(const mdns_record& rec, std::string& dest_addr)
{
    for(size_t i = 0; i < rec.data.size(); i++)
        dest_addr.append((i < rec.data.size() - 1) ? 
            std::to_string(static_cast<unsigned char>(rec.data[i])) + '.' : 
            std::to_string(static_cast<unsigned char>(rec.data[i])));

}

} // namespace discovery

