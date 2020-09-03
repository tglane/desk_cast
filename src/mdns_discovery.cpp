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

namespace mdns
{

constexpr uint16_t MDNS_RESPONSE_FLAG = 0x8400;
constexpr uint8_t MDNS_OFFSET_TOKEN = 0xC0;
constexpr size_t MDNS_RECORD_HEADER_LEN = 12;

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

static size_t read_fqdn(const std::vector<unsigned char>& data, size_t offset, std::string& result)
{
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

static mdns_res parse_mdns_answer(std::vector<unsigned char>& buffer)
{
    dns_header* dns = reinterpret_cast<dns_header*>(buffer.data());
    
    if(buffer.empty() || ntohs(dns->ans_count) == 0)
        throw std::invalid_argument("Not a valid mdns response.");
    
    mdns_res result;

    std::istringstream is(std::string(reinterpret_cast<char*>(buffer.data()), buffer.size()));
    is.exceptions(std::istream::failbit | std::istream::badbit | std::istream::eofbit);

    try {
        uint8_t u8;
        uint16_t u16;

        is.ignore(2); // Ignore id

        is.read(reinterpret_cast<char*>(&u16), 2); // Read flags
        if(ntohs(u16) != MDNS_RESPONSE_FLAG)
            throw std::invalid_argument("No valid mdns response flags");

        is.ignore(8); // qdcount, ancount, nscount, arcount

        size_t br = read_fqdn(buffer, static_cast<size_t>(is.tellg()), result.qname);
        if(br == 0)
            throw std::invalid_argument("");
        is.ignore(br);

        is.read(reinterpret_cast<char*>(&u16), 2); // qtype 
        result.qtype = ntohs(u16);

        is.ignore(2); // ignore qclass

        // TODO parsing of the records not working .. 
        // for(const auto& it : buffer)
        //     std::cout << it << std::endl;

        while(1)
        {
            is.exceptions(std::istream::goodbit);
            if(is.peek() == EOF)
                break;
            is.exceptions(std::istream::failbit | std::istream::badbit | std::istream::eofbit);

            mdns_record rec = {};
            rec.pos = static_cast<size_t>(is.tellg());

            is.read(reinterpret_cast<char*>(&u8), 1); // offset token
            if(u8 != MDNS_OFFSET_TOKEN)
                throw std::invalid_argument("Wrong mdns offset token");
            
            is.read(reinterpret_cast<char*>(&u8), 1); // offset value
            if(u8 >= buffer.size() || u8 + buffer[u8] >= buffer.size())
                throw std::invalid_argument("Wrong offset value");

            rec.name = std::string(reinterpret_cast<const char*>(&buffer[u8 + 1]), buffer[u8]);
            
            is.read(reinterpret_cast<char*>(&u16), 2); // type
            rec.type = ntohs(u16);
            is.ignore(6); // ignore qclass and ttl

            is.read(reinterpret_cast<char*>(&u16), 2); // data length
            is.ignore(ntohs(u16)); // ignore data
            rec.len = MDNS_RECORD_HEADER_LEN + ntohs(u16);

            result.records.push_back(std::move(rec));
        }

    } catch(std::istream::failure& e) {
        throw std::invalid_argument("No parseable response.");
    }

    return result;
}

std::vector<mdns_res> mdns_discovery(const std::string& record_name)
{
    bool stop = false;
    socketwrapper::UDPSocket q_sock(AF_INET);
    std::vector<mdns_res> res;

    // Set up mdns query
    char query[6536];
    dns_header* dns = reinterpret_cast<dns_header*>(&query);
    dns->id = 0x0000;
    dns->qr = 0;
    dns->opcode = 0;
    dns->aa = 0;
    dns->tc = 0;
    dns->rd = 1;
    dns->ra = 0;
    dns->z = 0;
    dns->ad = 0;
    dns->cd = 0;
    dns->rcode = 0;
    dns->q_count = htons(1);
    dns->ans_count = 0;
    dns->auth_count = 0;
    dns->add_count = 0;

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
                std::vector<unsigned char> buffer = q_sock.receive_vector<unsigned char>(4096, reinterpret_cast<sockaddr_in*>(&peer));

                try {
                    mdns_res tmp = parse_mdns_answer(buffer);
                    tmp.peer = peer;

                    res.push_back(std::move(tmp));
                } catch(std::exception& e) {
                    std::cout << e.what() << std::endl;
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

}
