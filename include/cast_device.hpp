#ifndef CAST_DEVICE_HPP
#define CAST_DEVICE_HPP

#include <memory>
#include <string>
#include <map>

#include <socketwrapper/SSLTCPSocket.hpp>

#include <mdns_discovery.hpp>

class cast_device
{
public:

    cast_device() = delete;
    cast_device(const cast_device&) = delete;
    cast_device(cast_device&&) = default;
    ~cast_device() = default;

    cast_device(const mdns::mdns_res& res, const char* ssl_cert, const char* ssl_key);
    
    bool connect();

private:

    bool send(const std::string_view nspace, const std::string_view dest_id, std::string_view payload) const;

    bool receive() const;

    std::unique_ptr<socketwrapper::SSLTCPSocket> m_sock_ptr;

    std::string m_name;                           // From PTR record

    std::string m_target;                         // From SRV record

    uint32_t m_port;                              // From SRV record

    std::string m_ip;                             // From A record

    std::map<std::string, std::string> m_txt;     // From TXT record

    uint64_t m_request_id = 0;

};

#endif
