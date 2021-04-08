#ifndef UPNP_MEDIA_RENDERER_HPP
#define UPNP_MEDIA_RENDERER_HPP

#include "upnp_device.hpp"
#include "utils.hpp"

namespace upnp
{

class media_renderer
{
public:

    explicit media_renderer(upnp_device& device)
        : m_device {device}
    {
        if(!m_device.connected())
        {
            // TODO Improve this
            for(uint8_t i = 0; !m_deivce.connect() && i < 5; ++i)
            if(!m_deivce.connected())
                throw std::runtime_error {"Can not connect to device."};
        }

        if(!m_device.app_available())
            throw std::runtime_error {"Device does not support Media Renderer."};
    }

    bool set_media(const utils::media_data& data)
    {
        // TODO
        return true;
    }

private:

    upnp_device& m_device;

};

} // namespace upnp

#endif

