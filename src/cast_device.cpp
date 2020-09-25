#include <cast_device.hpp>

#include <iostream>

using extensions::core_api::cast_channel::CastMessage;

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

    CastMessage msg;
    if(!this->receive(msg))
        return false;
    
    std::cout << "[DEBUG]: " << msg.namespace_() << std::endl;
    std::cout << "[DEBUG]: " << msg.payload_type() << std::endl;
    std::cout << "[DEBUG]: " << msg.payload_utf8() << std::endl;

    return true;
}

bool cast_device::send(const std::string_view nspace, const std::string_view dest_id, std::string_view payload) const
{
    CastMessage msg;
    msg.set_payload_type(msg.STRING);
    msg.set_protocol_version(msg.CASTV2_1_0);
    msg.set_namespace_(nspace.data());
    msg.set_source_id(source_id);
    msg.set_destination_id((dest_id.empty()) ? receiver_id : dest_id.data());

    msg.set_payload_utf8(payload.data());

    std::cout << "Test 1" << std::endl;
    uint32_t len = htonl(msg.ByteSize());

    std::unique_ptr<char[]> data = std::make_unique<char[]>(4 + len);
    std::memcpy(&data[0], &len, sizeof(uint32_t));

    std::cout << "Test 2" << std::endl;    
    if(!msg.SerializeToArray(&data[4], len))
        return false;
    std::cout << "Test 3" << std::endl;

    try {
        m_sock_ptr->write<char>(data.get(), 4 + len);
    } catch(std::exception& e) {
        return false;
    }

    std::cout << "Test 4" << std::endl;
    return true;
}

bool cast_device::receive(CastMessage& dest_msg) const
{
    size_t bytes;
    std::unique_ptr<char[]> pkglen = m_sock_ptr->read<char>(4, &bytes);
    uint32_t len = ntohl(*reinterpret_cast<uint32_t*>(pkglen.get()));

    std::unique_ptr<char[]> msg_buf;
    try {
        msg_buf = m_sock_ptr->read<char>(len);
    } catch(std::exception& e) {
        return false;
    }
    
    if(!dest_msg.ParseFromArray(msg_buf.get(), len))
        return false;
    else
        return true;
}
