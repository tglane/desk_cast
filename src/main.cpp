#include <iostream>
#include <memory>
#include <thread>
#include <signal.h>

#include "socketwrapper.hpp"

#include "dial_discovery.hpp"
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

static bool launch_app_on_device(device& dev)
{
    // TODO Change this to work with all device types
    json media_payload;
    try {
        media_payload["media"]["contentId"] = "http://" + utils::get_local_ipaddr() + ":5770/index.m3u8";
        media_payload["media"]["contentType"] = "application/x-mpegurl";
        media_payload["media"]["streamType"] = "LIVE";
        media_payload["type"] = "LOAD";
    } catch(std::runtime_error&) {
        return false;
    }

    return dev.launch_app("CC1AD845", std::move(media_payload));
}

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
        uint16_t port;
        try {
            discovery::parse_location(it.at("LOCATION"), addr, path, port);
        } catch(std::exception& e) {
            continue;
        }

        net::tcp_connection<net::ip_version::v4> conn {addr, port};

        std::string req_str {"GET "};
        (((req_str += path) += " HTTP/1.1\r\nHOST: ") += addr) +=  "\r\n\r\n";
        std::cout << req_str << '\n';

        conn.send(std::string_view {req_str});

        // while(!conn.bytes_available())
        //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::vector<char> buffer = conn.read<char>(4096);
        std::cout << buffer.data() << std::endl;

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
    sigset_t sigset;
    std::atomic<bool> run_condition {true};
    block_signals(&sigset);
    std::future<int> signal_handler = std::async(std::launch::async, [&run_condition, &sigset]()
    {
        int signum = 0;
        sigwait(&sigset, &signum);
        run_condition.store(false);
        std::cout << "Shutting down ...\n";

        // Quick and dirty to stop waiting for accept in the web server
        // Maybe improve this later
        try {
            net::tcp_connection<net::ip_version::v4> sock {"127.0.0.1", WEBSERVER_PORT};
        } catch(std::runtime_error& err) {}

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

    std::cout << (launch_app_on_device(*device) ? "Launched" : "Launch error") << std::endl;

    // Wait for signal and shut down all threads
    int signal = signal_handler.get();
    for(auto& t : worker)
    {
        t.join();
        std::cout << "Worker returned" << std::endl;
    }

    return EXIT_SUCCESS;
}
