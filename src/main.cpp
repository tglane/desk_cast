#include <iostream>
#include <memory>

#include <socketwrapper/UDPSocket.hpp>
#include <socketwrapper/TCPSocket.hpp>
#include <socketwrapper/SSLTCPSocket.hpp>

#include <dial_discovery.hpp>
#include <mdns_discovery.hpp>
#include <cast_device.hpp>

#define SSL_CERT "/etc/ssl/certs/cert.pem"
#define SSL_KEY "/etc/ssl/private/key.pem"

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

    std::cout << "Searching for casteable devices in your local network...\n";
    std::vector<mdns_res> responses = mdns_discovery("_googlecast._tcp.local");
    std::cout << "Received " << responses.size() << " responses from potential googlecast devices.\n";
    
    if(responses.size() == 0)
        return;
    
    std::vector<googlecast::cast_device> devices;
    devices.reserve(responses.size());
    uint32_t select;
    for(size_t i = 0; i < responses.size(); i++)
    {
        googlecast::cast_device& dev = devices.emplace_back(responses[i], SSL_CERT, SSL_KEY);
        std::cout << i << " | " << dev.get_name() << '\n';
    }
    std::cout << "\nSelect the device you want to connect to:\n>> ";
    std::cin >> select;

    if(0 > select || select >= devices.size())
        select = 0; // Default is 0

    googlecast::cast_device& dev = devices[select];
    if(!dev.connect())
        return;
    
    dev.launch_app("CC1AD845");

}

int main()
{
    // main_dial();

    main_mdns();
}
