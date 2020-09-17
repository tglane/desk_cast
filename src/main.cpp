#include <iostream>
#include <memory>

#include <socketwrapper/UDPSocket.hpp>
#include <socketwrapper/TCPSocket.hpp>

#include <dial_discovery.hpp>
#include <mdns_discovery.hpp>
#include <cast_device.hpp>

void main_dial() 
{
    using namespace dial;

    std::vector<ssdp_res> responses = upnp_discovery();
    for(const auto& it : responses)
    {
        std::cout << "--------------------\n";
        for(const auto& mit : it)
            std::cout << mit.first << " => " << mit.second << '\n';
        std::cout << "--------------------\n" << std::endl;

        std::string addr;
        std::string path;
        int port;
        try {
            parse_location(it.at("LOCATION"), addr, path, port);
        } catch(std::exception& e) {
            continue;
        }

        socketwrapper::TCPSocket conn(AF_INET);
        conn.connect(port, addr);

        std::string req_str("GET ");
        (((req_str += path) += " HTTP/1.1\r\nHOST: ") += addr) +=  "\r\n\r\n";
        std::cout << req_str << '\n';

        conn.write<char>(req_str.data(), req_str.size());

        while(!conn.bytes_available())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::unique_ptr<char[]> buffer = conn.read<char>(4096);
        std::cout << buffer.get() << std::endl;

        // TODO Parse and check if there is a service of type dial
        // parse_services();
    }
}

void main_mdns()
{
    using namespace mdns;

    std::vector<mdns_res> responses = mdns_discovery("_googlecast._tcp.local");
    
    std::vector<cast_device> devices;
    devices.reserve(responses.size());

    std::cout << "Received " << responses.size() << " responses from potential googlecast devices.\n";

    for(const auto& it : responses)
    {
        cast_device& chr = devices.emplace_back();

        // Get all information to "contsturct" a chromecast device from the response
        for(const auto& rec : it.records)
        {
            switch(rec.type)
            {
                case 1:
                    // A record
                    parse_a_record(rec, chr.ip);
                    break;
                case 5:
                    // CNAME record
                    break;
                case 12:
                    // PTR record
                    parse_ptr_record(rec, chr.name);
                    break;
                case 16:
                    // TXT record
                    parse_txt_record(rec, chr.txt);
                    break;
                case 28:
                    // AAAA record
                    break;
                case 33:
                    // SRV record
                    parse_srv_record(rec, chr.port, chr.target);
                    break;
                default:
                    // Skip all not expected record types
                    break;
            }
        }
    }
}

int main()
{
    // main_dial();

    main_mdns();
}
