#include <memory>
#include <thread>
#include <csignal>

#include "fmt/format.h"

#include "socketwrapper.hpp"

#include "ssdp_discovery.hpp"
#include "mdns_discovery.hpp"
#include "device.hpp"
#include "cast_device.hpp"
#include "default_media_receiver.hpp"
#include "upnp_device.hpp"
#include "media_renderer.hpp"
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
    std::vector<discovery::mdns_res> mdns = discovery::mdns("_googlecast._tcp.local");
    if(!mdns.empty())
    {
        for(const auto& res : mdns)
        {
            devices.push_back(std::make_unique<googlecast::cast_device>(res, std::string_view {SSL_CERT}, std::string_view {SSL_KEY}));
        }
    }

    std::vector<discovery::ssdp_res> ssdp = discovery::ssdp("urn:schemas-upnp-org:device:MediaRenderer:1");
    if(!ssdp.empty())
    {
        for(auto& res : ssdp)
        {
            devices.push_back(std::make_unique<upnp::upnp_device>(static_cast<discovery::ssdp_res&&>(res)));
        }
    }

    return devices;
}

static device_ptr& select_device(std::vector<device_ptr>& devices)
{
    // TDOD Do this in a CLI class/module

    fmt::print("Detected {} device(s) in your local network.\n-------------------------------\n", devices.size());
    for(size_t i = 0; i < devices.size(); i++)
    {
        fmt::print("{} | {}\n", i, devices[i]->get_name());
    }
    fmt::print("\nSelect the device you want to connect to:\n>> ");

    size_t selected;
    std::cin >> selected;
    if(selected >= devices.size())
        selected = 0; // Default selection

    return devices[selected];
}

// TODO Remove if everything is implemented correctly so that the real program structure can be used for upnp devices too
static void main_upnp()
{
    std::atomic<bool> run_condition {true};
    std::thread webserver_t {[&run_condition]() {
        http::webserver server {WEBSERVER_PORT, SSL_CERT, SSL_KEY};
        server.serve(run_condition);
    }};
    webserver_t.detach();

    std::vector<discovery::ssdp_res> responses = discovery::ssdp("urn:schemas-upnp-org:device:MediaRenderer:1");
    for(const auto& it : responses)
    {
        upnp::upnp_device device {it};
        if(!device.connect())
        {
            fmt::print("Unable to connect the device\n");
            return;
        }
        fmt::print("Connected to UPNP device\n");

        upnp::media_renderer mr {device};
        bool launch_result = mr.set_media(utils::media_data {
            fmt::format("http://{}:5770/test_video.mp4", utils::get_local_ipaddr()),
            "video/mp4"
        });
        if(!launch_result)
        {
            fmt::print("Launch error\n");
            return;
        }
        fmt::print("Launched\n");

        for(;;)
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
    // TODO This is just to develop the upnp device connection
    // main_upnp();
    // return 0;

    sigset_t sigset;
    std::atomic<bool> run_condition {true};
    block_signals(&sigset);
    std::future<int> signal_handler = std::async(std::launch::async, [&run_condition, &sigset]()
    {
        int signum = 0;
        sigwait(&sigset, &signum);
        run_condition.store(false);
        fmt::print("Shutting down...\n");

        // Quick and dirty to stop waiting for accept in the web server
        try {
            net::tcp_connection<net::ip_version::v4> sock {"127.0.0.1", WEBSERVER_PORT};
        } catch(std::runtime_error& err) {}

        return signum;
    });

    fmt::print("Scanning network for cast-enabled devices...\n");
    std::vector<device_ptr> device_list = get_devices();
    if(device_list.empty())
    {
        fmt::print("No devices found...\nPress Ctrl+C to exit.\n");
        signal_handler.get();
        return EXIT_SUCCESS;
    }

    device_ptr& device = select_device(device_list);
    if(!device->connect())
    {
        fmt::print("Connection failed...\nPress Ctrl+C to exit.\n");
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

    googlecast::default_media_receiver dmr {*reinterpret_cast<googlecast::cast_device*>(device.get())};
    bool launch_flag = dmr.set_media(utils::media_data {
        fmt::format("http://{}:5770/index.m3u8", utils::get_local_ipaddr()),
        "application/x-mpegurl"
    });
    fmt::print("Status: {}", (launch_flag) ? "Launched" : "Launch error");

    // Wait for signal and shut down all threads
    int signal = signal_handler.get();
    for(auto& t : worker)
    {
        t.join();
        fmt::print("Worker returned\n");
    }

    return EXIT_SUCCESS;
}
