#ifndef DEFAULT_MEDIA_RECEIVER_HPP
#define DEFAULT_MEDIA_RECEIVER_HPP

#include <memory>

#include "cast_device.hpp"

namespace googlecast
{

struct media_data
{
    // TODO Maybe add more states if necessary
    std::string url;
    std::string mime_type;
};

class default_media_receiver
{
    enum class dmr_status
    {
        idle,
        streaming,
        closed
    };

public:

    explicit default_media_receiver(cast_device& device)
        : m_status {dmr_status::idle}, m_device {device}
    {
        if(!m_device.connected())
        {
            // TODO Improve this
            for(uint8_t i = 0; !m_device.connect() && i < 5; ++i);
            if(!m_device.connected())
                throw std::runtime_error {"Can not connect to device."};
        }

        if(!m_device.app_available("CC1AD845"))
            throw std::runtime_error {"Device does not support Default Media Receiver."};

        // TODO Maybe launch the dmr but without any content?
    }

    bool set_media(const media_data& data)
    {
        if(m_status == dmr_status::closed)
            throw std::runtime_error {"Connection already closed."};

        json payload;
        payload["media"]["contentId"] = data.url;
        payload["media"]["contentType"] = data.mime_type;
        payload["media"]["streamType"] = "LIVE"; // TODO Figure out how to set this correct for all content types
        payload["type"] = "LOAD";

        return m_device.launch_app("CC1AD845", static_cast<json&&>(payload));
    }

private:

    dmr_status m_status;

    cast_device& m_device;

};

} // namespace googlecast

#endif
