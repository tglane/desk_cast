#include <iostream>
#include <memory>
#include <thread>
#include <signal.h>

#include "socketwrapper/UDPSocket.hpp"
#include "socketwrapper/TCPSocket.hpp"
#include "socketwrapper/SSLTCPSocket.hpp"

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

static bool launch_app_on_device(device& dev, std::string_view app_id)
{
    // TODO
    return true;
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
        int port;
        try {
            discovery::parse_location(it.at("LOCATION"), addr, path, port);
        } catch(std::exception& e) {
            continue;
        }

        socketwrapper::TCPSocket conn {AF_INET};
        conn.connect(port, addr);

        std::string req_str {"GET "};
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
        return; // TODO Return/throw error
    
    std::vector<googlecast::cast_device> devices;
    devices.reserve(responses.size());
    uint32_t select;
    for(size_t i = 0; i < responses.size(); i++)
    {
        googlecast::cast_device& dev = devices.emplace_back(responses[i], std::string_view {SSL_CERT}, std::string_view {SSL_KEY});
        std::cout << i << " | " << dev.get_name() << '\n';
    }
    std::cout << "\nSelect the device you want to connect to:\n>> ";
    std::cin >> select;
    if(0 > select || select >= devices.size())
        select = 0; // Default is 0

    googlecast::cast_device& dev = devices[select];
    if(!dev.connect())
        return; // TODO Return/throw error

    std::string content_id;
    try {
        content_id = "http://" + utils::get_local_ipaddr() + ":5770/index.m3u8";
    } catch(std::runtime_error&) {
        return; // TODO Return/throw error
    }

    json media_payload;
    media_payload["media"]["contentId"] = content_id;
    media_payload["media"]["contentType"] = "application/x-mpegurl";
    media_payload["media"]["streamType"] = "LIVE";
    media_payload["type"] = "LOAD";

    if(!dev.launch_app("CC1AD845", std::move(media_payload)))
    {
        std::cout << "Launch error" << std::endl;
        return; // TODO Return/throw error
    }

    std::cout << "Launched" << std::endl;
}

static void init_webserver(std::atomic<bool>& run_condition)
{
    http::webserver server {WEBSERVER_PORT, SSL_CERT, SSL_KEY};
    server.serve(run_condition);
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

    std::vector<std::thread> worker;
    worker.reserve(3);
    worker.emplace_back(init_webserver, std::ref(run_condition));
    worker.emplace_back(main_mdns);
    worker.emplace_back(main_dial);
    // worker.emplace_back(init_capture, std::ref(run_condition));

    // Wait for signal and shut down all threads
    int signal = signal_handler.get();
    for(auto& t : worker)
    {
        t.join();
        std::cout << "Worker returned" << std::endl;
    }

    return EXIT_SUCCESS;
}
