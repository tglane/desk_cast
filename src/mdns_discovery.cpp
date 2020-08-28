#include <mdns_discovery.hpp>

#include <socketwrapper/UDPSocket.hpp>

#include <memory>
#include <thread>
#include <future>
#include <chrono>

#include <iostream>

namespace mdns
{



//DNS header structure
struct dns_header
{
    unsigned short id; // identification number
 
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
 
    unsigned short q_count; // number of question entries
    unsigned short ans_count; // number of answer entries
    unsigned short auth_count; // number of authority entries
    unsigned short add_count; // number of resource entries
};
 
//Constant sized fields of query structure
struct dns_question
{
    unsigned short qtype;
    unsigned short qclass;
};
 

//Structure of a Query
struct dns_query
{
    unsigned char *name;
    struct dns_question *ques;
};

static void change_to_dns_name_format(char* dns, const char* host)
{
    int lock = 0 , i;
    strcat((char*)host,".");
     
    for(i = 0 ; i < strlen((char*)host) ; i++) 
    {
        if(host[i]=='.') 
        {
            *dns++ = i-lock;
            for(;lock<i;lock++) 
            {
                *dns++=host[lock];
            }
            lock++; //or lock=i+1;
        }
    }
    *dns++='\0';
}

std::vector<std::string> mdns_query(const std::string& record_name)
{
    bool stop = false;
    // const char* query = "_googlecast._tcp.local";
    socketwrapper::UDPSocket q_sock(AF_INET);
    std::vector<std::string> res;


    char query[6536], *qname;
    dns_header* dns = (dns_header*) &query;
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

    qname = (char*) &query[sizeof(dns_header)];
    change_to_dns_name_format(qname, record_name.data());

    dns_question* qinfo = (dns_question*) &query[sizeof(dns_header) + (strlen((const char*)qname) + 1)];
    qinfo->qtype = htons(12);
    qinfo->qclass = htons(1);

    // Set up query
    q_sock.bind("0.0.0.0", 5353);
    q_sock.send_to<char>(query, sizeof(dns_header) + (strlen((const char*)qname)+1) + sizeof(dns_question), QUERY_PORT, QUERY_IP);

    auto fut = std::async(std::launch::async, [&stop, &q_sock, &res]() 
    {
        while(!stop)
        {
            if(q_sock.bytes_available())
            {
                // std::unique_ptr<char[]> buffer = q_sock.receive<char>(1024, nullptr);
                std::vector<char> buffer = q_sock.receive_vector<char>(1024, nullptr);

                std::cout << "[DEBUG] " << buffer.size() << std::endl;
                res.emplace_back(buffer.data());
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
