#include "ssdp_discovery.hpp"

#include "socketwrapper/UDPSocket.hpp"
#include "socketwrapper/TCPSocket.hpp"

#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <stdexcept>

namespace discovery
{

ssdp_location parse_location(std::string_view view)
{
    ssdp_location parsed;

    std::string::size_type tmp = view.find(':');
    view.remove_prefix(tmp + 3);

    parsed.ip = std::string {view.data(), view.find(':')};

    tmp = view.find(':') + 1;
    std::string_view port_view {view.data() + tmp, view.find('/') - tmp};
    parsed.port = std::stoi(port_view.data());

    tmp = view.find('/');
    parsed.path = std::string {view.data() + tmp, view.size() - tmp};

    return parsed;
}

ssdp_res parse_request(std::string_view view)
{
    ssdp_res res;

    view = {view.data() + view.find("\r\n") + 2};
    while(view != "\r\n")
    {
        size_t sep = view.find(':');
        size_t endl = view.find("\r\n");
        if(sep == std::string::npos || endl == std::string::npos)
            break;

        std::string_view key {view.data(), sep};
        std::string_view val {view.data() + sep + 2, endl - (sep + 2)};
        if(key == "LOCATION")
            res.location = parse_location(val);
        else if(key == "CACHE_CONTROL")
            res.cache_control = val;
        else if(key == "SERVER")
            res.server = val;
        else if(key == "USN")
            res.usn = val;
        else if(key == "ST")
            res.st = val;
        else if(key == "WAKEUP")
            res.wakeup = val;

        view = std::string_view {view.data() + endl + 2};
    }

    return res;
}

std::vector<ssdp_res> ssdp(const std::string& service_type)
{
    using namespace std::chrono;

    bool stop = false;
    std::string msg = ("M-SEARCH * HTTP/1.1\r\nHost: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 5\r\nST: " + service_type + "\r\n\r\n");
    socketwrapper::UDPSocket d_sock {socketwrapper::ip_version::v4};
    std::vector<ssdp_res> responses;

    d_sock.bind("0.0.0.0", 1900);
    d_sock.send_to<char>(msg.data(), msg.size(), DISCOVERY_PORT, DISCOVERY_IP);

    auto fut = std::async(std::launch::async, [&stop, &d_sock, &responses]() 
    {
        while(!stop)
        {
            if(d_sock.bytes_available())
            {
                std::unique_ptr<char[]> buffer = d_sock.receive<char>(d_sock.bytes_available(), nullptr);

                responses.push_back(parse_request(buffer.get()));
            }
        }
    });

    // Wait for x seconds before waiting for the async function to join
    std::this_thread::sleep_for(milliseconds(DISCOVERY_TIME));
    stop = true;
    fut.get();

    d_sock.close();
    return responses;
}

} // namespace discovery
