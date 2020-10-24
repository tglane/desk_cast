#include <iostream>
#include <memory>
#include <future>

#include <socketwrapper/UDPSocket.hpp>
#include <socketwrapper/TCPSocket.hpp>
#include <socketwrapper/SSLTCPSocket.hpp>

#include <dial_discovery.hpp>
#include <mdns_discovery.hpp>
#include <cast_device.hpp>
#include <cast_app.hpp>

#include <webserver.hpp>

#define SSL_CERT "/etc/ssl/certs/cert.pem"
#define SSL_KEY "/etc/ssl/private/key.pem"

static void main_dial() 
{
    std::vector<discovery::ssdp_res> responses = discovery::upnp_discovery();
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
            discovery::parse_location(it.at("LOCATION"), addr, path, port);
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

static void main_mdns()
{
    std::cout << "Searching for casteable devices in your local network...\n";
    std::vector<discovery::mdns_res> responses = discovery::mdns_discovery("_googlecast._tcp.local");
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


    std::future<void> cast_bringup = std::async(std::launch::async, [&devices, select]() {

        googlecast::cast_device& dev = devices[select];
        dev.close_app();
        if(!dev.connect())
            return;

        try {
            googlecast::cast_app& app = dev.launch_app("CC1AD845");
            // TODO If this returns false, the media is not set so exit the execution
            app.set_media();
        } catch(std::runtime_error& e) {
            std::cout << "[ERROR_LAUNCH_APP]:\n" << e.what() << std::endl;
            return;
        } catch(...) {
            std::cout << "..." << std::endl;
            return;
        }
    });

    ssl_webserver server(5770, SSL_CERT, SSL_KEY);
    server.serve();

    // JUST TO CHECK THE CONNECTION
    // uint64_t cnt = 0;
    // while(1)
    // {
    //     std::cout << dev.get_status().dump(2) << "\n--------------------------------------" << cnt << '\n' << std::endl;
    //     cnt++;
    //     std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    // }

}

int main()
{
    // TODO Split up code
    // -> Use main_dial/main_mdns to get the device to connect to
    // -> When we have a device, launch screenrecorder in background thread (not implemented yet) (loop)
    // -> Launch the app on the selected device in the background
    // -> Start webserver serving the cast devices in the main thread (loop)

    // main_dial();

    main_mdns();

}
