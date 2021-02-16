#include <iostream>
#include <memory>
#include <thread>
#include <signal.h>

#include "socketwrapper/UDPSocket.hpp"
#include "socketwrapper/TCPSocket.hpp"
#include "socketwrapper/SSLTCPSocket.hpp"

#include "ssdp_discovery.hpp"
#include "mdns_discovery.hpp"
#include "device.hpp"
#include "cast_device.hpp"
#include "utils.hpp"

#include "http/webserver.hpp"

// Set path to certificate and READEABLE key file used by the webserver and the cast device connector
#define SSL_CERT "./cert.pem"
#define SSL_KEY "./key.pem"

#define WEBSERVER_PORT 5770

static std::vector<std::unique_ptr<device>> get_devices()
{
    std::vector<std::unique_ptr<device>> devices;

    // TODO Extend this to get all device types available

    // Get googlecast devices via mdns request
    std::vector<discovery::mdns_res> mdns = discovery::mdns_discovery("_googlecast._tcp.local");
    if(mdns.size() > 0)
    {
        devices.reserve(devices.size() + mdns.size());
        for(size_t i = 0; i < mdns.size(); i++)
            devices.push_back(std::make_unique<googlecast::cast_device>(mdns[i], std::string_view {SSL_CERT}, std::string_view {SSL_KEY}));
    }

    return devices;
}

static device_ptr& select_device(std::vector<device_ptr>& devices)
{
    size_t selected;
    for(size_t i = 0; i < devices.size(); i++)
    {
        std::cout << i << " | " << devices[i]->get_name() << '\n';
    }
    std::cout << "\nSelect the device you want to connect to:\n>> ";
    std::cin >> selected;
    if(0 > selected || selected >= devices.size())
        selected = 0; // Default selection

    return devices[selected];
}

static bool launch_app_on_device(device* dev)
{
    // TODO Change this to work with all device types
    return googlecast::start_live_stream(*static_cast<googlecast::cast_device*>(dev), "http://" + utils::get_local_ipaddr() + ":5770/index.m3u8");
}

static void main_dial() 
{
    // std::vector<discovery::ssdp_res> responses = discovery::ssdp("urn:schemas-upnp-org:device:MediaServer:1");
    std::vector<discovery::ssdp_res> responses = discovery::ssdp("urn:schemas-upnp-org:device:MediaRenderer:1");
    for(const auto& it : responses)
    {
        std::cout << "--------------------\n"
        << it.location.ip << '\n' << '\n';
        // << it.location.path << '\n'
        // << it.location.port << '\n'
        // << it.server << '\n'
        // << it.usn << '\n';

        socketwrapper::TCPSocket conn {AF_INET};
        conn.connect(it.location.port, it.location.ip);

        std::string req_str = ("GET " + it.location.path + " HTTP/1.1\r\nHOST: " + it.location.ip + ":" + std::to_string(it.location.port) +  "\r\n\r\n");
        std::cout << req_str << '\n';

        conn.write<char>(req_str.data(), req_str.size());

        while(!conn.bytes_available())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::unique_ptr<char[]> buffer = conn.read<char>(4096);
        std::cout << buffer.get() << '\n';

        std::cout << "--------------------\n" << std::endl;
        // TODO Parse and check if there is a service of type dial
        // parse_services();
    }
}

static void block_signals(sigset_t* sigset)
{
    sigemptyset(sigset);
    sigaddset(sigset, SIGINT);
    sigaddset(sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, sigset, nullptr);
}

int main()
{
    main_dial();
    return 0;

    sigset_t sigset;
    std::atomic<bool> run_condition {true};
    block_signals(&sigset);
    std::future<int> signal_handler = std::async(std::launch::async, [&run_condition, &sigset]()
    {
        int signum = 0;
        sigwait(&sigset, &signum);
        run_condition.store(false);
        std::cout << "Shutting down ...\n";

        // TODO Change to async I/O
        // Quick and dirty to stop waiting for accept in the web server
        socketwrapper::TCPSocket sock {AF_INET};
        try {
            sock.connect(WEBSERVER_PORT, "127.0.0.1");
        } catch(socketwrapper::SocketConnectingException& e) {}

        return signum;
    });

    // TODO Split up code
    // -> Create condition to close app...
    // -> Use main_dial/main_mdns to get the device to connect to
    // -> When we have a device, launch screenrecorder in background thread (not implemented yet) (loop)
    // -> Launch webserver serving the cast devices in background thread (loop)
    // -> Launch the app on the selected device in main thread

    std::vector<device_ptr> device_list = get_devices();
    if(device_list.size() == 0)
    {
        std::cout << "No devices found...\nPress Ctrl+C to exit.\n";
        signal_handler.get();
        return EXIT_SUCCESS;
    }

    device_ptr& device = select_device(device_list);
    if(!device->connect())
    {
        std::cout << "Connection failed...\nPress Ctrl+C to exit.\n";
        signal_handler.get();
        return EXIT_FAILURE;
    }

    std::vector<std::thread> worker;
    worker.reserve(2);
    worker.emplace_back([&run_condition]() {
        http::webserver server {WEBSERVER_PORT, SSL_CERT, SSL_KEY};
        server.serve(run_condition);
    });
    // worker.emplace_back(init_capture, std::ref(run_condition));

    std::cout << (launch_app_on_device(device.get()) ? "Launched" : "Launch error") << std::endl;

    // Wait for signal and shut down all threads
    int signal = signal_handler.get();
    for(auto& t : worker)
    {
        t.join();
        std::cout << "Worker returned" << std::endl;
    }

    return EXIT_SUCCESS;
}
