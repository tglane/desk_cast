#ifndef DEFAULT_MEDIA_RECEIVER_HPP
#define DEFAULT_MEDIA_RECEIVER_HPP

#include <memory>
#include <iostream>

#include "cast_device.hpp"

namespace googlecast
{

struct media_data
{
    // TODO
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

    explicit default_media_receiver(cast_device&& device)
        : m_status {dmr_status::idle}, m_device {std::make_unique<cast_device>(std::move(device))}
    {
        // TODO Maybe launch the dmr but without any content?
        // TODO Check if the device is connected
        std::cout << "Hello" << std::endl;
        if(!m_device->app_available("CC1AD845"))
            throw std::runtime_error {"Device does not support Default Media Receiver."};
        std::cout << "Hallo" << std::endl;
    }

    explicit default_media_receiver(std::unique_ptr<cast_device>&& device_ptr)
        : m_status {dmr_status::idle}, m_device {static_cast<std::unique_ptr<cast_device>&&>(device_ptr)}
    {
        // TODO Check if the device is connected
        std::cout << "ptr hello" << std::endl;
        if(!m_device->app_available("CC1AD845"))
            throw std::runtime_error {"Device does not support Default Media Receiver."};
        std::cout << "ptr Hallo" << std::endl;
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

        return m_device->launch_app("CC1AD845", static_cast<json&&>(payload));
    }

    std::unique_ptr<cast_device>&& get_device()
    {
        // TODO I dont know if this is useful
        m_status = dmr_status::closed;
        return static_cast<std::unique_ptr<cast_device>&&>(m_device);
    }

private:

    dmr_status m_status;

    std::unique_ptr<cast_device> m_device;

};

} // namespace googlecast

#endif

