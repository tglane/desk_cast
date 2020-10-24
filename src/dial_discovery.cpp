#include <dial_discovery.hpp>

#include <socketwrapper/UDPSocket.hpp>
#include <socketwrapper/TCPSocket.hpp>

#include <memory>
#include <thread>
#include <future>
#include <chrono>
#include <stdexcept>

namespace discovery
{

ssdp_res parse_request(std::string_view view)
{
    ssdp_res res;

    view = std::string_view(view.data() + view.find("\r\n") + 2);
    while(view != "\r\n")
    {
        size_t sep = view.find(':'),endl = view.find("\r\n");
        // TODO error checking

        res[std::string(view.data(), sep)] = std::string(view.data() + sep + 2, endl - ( sep + 2));

        view = std::string_view(view.data() + endl + 2);
    }

    return res;
}

void parse_location(const std::string& location, std::string& addr, std::string& path, int& port)
{
    // TODO Throw std::invalid_argument if location is not a valid location string

    std::string_view view(location.data() + location.find(':') + 3);

    addr = std::string(view.data(), view.find(':'));

    std::string_view port_view(view.data() + view.find(':') + 1, view.find('/'));
    port = std::stoi(port_view.data());

    path = std::string(port_view.data() + port_view.find('/'));
}

std::vector<ssdp_res> upnp_discovery()
{
    using namespace std::chrono;

    bool stop = false;
    const char* msg = "M-SEARCH * HTTP/1.1\r\n Host: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 3\r\nST: urn:dial-multiscreen-org:service:dial:1\r\n\r\n";
    socketwrapper::UDPSocket d_sock(AF_INET);
    std::vector<ssdp_res> responses;

    d_sock.bind("0.0.0.0", 1900);
    d_sock.send_to<char>(msg, strlen(msg), DISCOVERY_PORT, DISCOVERY_IP);

    auto fut = std::async(std::launch::async, [&stop, &d_sock, &responses]() 
    {
        while(!stop)
        {
            if(d_sock.bytes_available())
            {
                std::unique_ptr<char[]> buffer = d_sock.receive<char>(d_sock.bytes_available(), nullptr);

                responses.push_back(std::move(parse_request(buffer.get())));
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
