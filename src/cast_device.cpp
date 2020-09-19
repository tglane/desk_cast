#include <cast_device.hpp>

#include "../protos/cast_channel.pb.h"

#include <iostream>

const char* source_id = "sender-0";
const char* receiver_id = "receiver-0";

cast_device::cast_device(const mdns::mdns_res& res, const char* ssl_cert, const char* ssl_key)
    : m_sock_ptr(std::make_unique<socketwrapper::SSLTCPSocket>(AF_INET, ssl_cert, ssl_key))
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    for(const auto& rec : res.records)
    {
        switch(rec.type)
        {
            case 1:
                // A record
                parse_a_record(rec, this->m_ip);
                break;
            case 5:
                // CNAME record
                break;
            case 12:
                // PTR record
                parse_ptr_record(rec, this->m_name);
                break;
            case 16:
                // TXT record
                parse_txt_record(rec, this->m_txt);
                break;
            case 28:
                // AAAA record
                break;
            case 33:
                // SRV record
                parse_srv_record(rec, this->m_port, this->m_target);
                break;
            default:
                // Skip all not expected record types
                break;
        }
    }
}

bool cast_device::connect()
{
    try {
        m_sock_ptr->connect(m_port, m_ip);
    } catch(std::exception& e) {
        return false;
    }

    this->send("urn:x-cast:com.google.cast.tp.connection", "", 
        R"({ "type": "CONNECT" })");

    this->send("urn:x-cast:com.google.cast.receiver", "", 
        R"({ "type": "GET_STATUS", "requestId": 0 })");

    this->receive();

    return true;
}

bool cast_device::send(const std::string_view nspace, const std::string_view dest_id, std::string_view payload) const
{
    using namespace extensions::core_api;

    cast_channel::CastMessage msg;
    msg.set_payload_type(msg.STRING);
    msg.set_protocol_version(msg.CASTV2_1_0);
    msg.set_namespace_(nspace.data());
    msg.set_source_id(source_id);
    msg.set_destination_id((dest_id.empty()) ? receiver_id : dest_id.data());

    msg.set_payload_utf8(payload.data());

    return true;
}

bool cast_device::receive() const
{
    using extensions::core_api::cast_channel::CastMessage;

    std::unique_ptr<char[]> pkglen = m_sock_ptr->read<char>(4);
    uint32_t len;
    std::memcpy(&len, pkglen.get(), sizeof(len));
    len = ntohl(len);
    std::cout << "PKGLEN: " << len << std::endl;

    std::unique_ptr<char[]> msg_buf = m_sock_ptr->read<char>(88);
    CastMessage msg;
    if(!msg.ParseFromString(msg_buf.get()))
        std::cout << "could not parse message" << std::endl;
    else
        std::cout << "could parse" << std::endl;
    
}
