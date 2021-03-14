#include "dial_discovery.hpp"

#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <stdexcept>

#include "socketwrapper.hpp"

namespace discovery
{

ssdp_res parse_request(std::string_view view)
{
    ssdp_res res;

    view = {view.data() + view.find("\r\n") + 2};
    while(view != "\r\n")
    {
        size_t sep = view.find(':'),endl = view.find("\r\n");
        // TODO error checking

        res[std::string(view.data(), sep)] = std::string {view.data() + sep + 2, endl - ( sep + 2)};

        view = std::string_view {view.data() + endl + 2};
    }

    return res;
}

void parse_location(const std::string& location, std::string& addr, std::string& path, uint16_t& port)
{
    // TODO Throw std::invalid_argument if location is not a valid location string

    std::string_view view {location.data() + location.find(':') + 3};

    addr = std::string {view.data(), view.find(':')};

    std::string_view port_view {view.data() + view.find(':') + 1, view.find('/')};
    port = std::stoi(port_view.data());

    path = std::string {port_view.data() + port_view.find('/')};
}

std::vector<ssdp_res> upnp_discovery()
{
    using namespace std::chrono;

    bool stop = false;
    std::string_view msg  {"M-SEARCH * HTTP/1.1\r\n Host: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 3\r\nST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n"};
    std::vector<ssdp_res> responses;
    net::udp_socket<net::ip_version::v4> d_sock {"0.0.0.0", 1900};
    d_sock.send(DISCOVERY_IP, DISCOVERY_PORT, msg);

    auto fut = std::async(std::launch::async, [&stop, &d_sock, &responses]() 
    {
        while(!stop)
        {
            auto [buffer, peer] = d_sock.read<char>(4096);
            responses.push_back(parse_request(buffer.data()));
        }
    });

    // Wait for x seconds before waiting for the async function to join
    std::this_thread::sleep_for(milliseconds(DISCOVERY_TIME));
    stop = true;
    fut.get();

    return responses;
}

} // namespace discovery
