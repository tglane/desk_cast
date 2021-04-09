#ifndef UPNP_MEDIA_RENDERER_HPP
#define UPNP_MEDIA_RENDERER_HPP

#include "upnp_device.hpp"
#include "utils.hpp"

namespace upnp
{

class media_renderer
{
    // TODO Implement media renderer status system as shown in upnp description
public:

    explicit media_renderer(upnp_device& device)
        : m_device {device}
    {
        if(!m_device.connected())
        {
            // TODO Improve this
            for(uint8_t i = 0; !m_device.connect() && i < 5; ++i)
            if(!m_device.connected())
                throw std::runtime_error {"Can not connect to device."};
        }

        // TODO Set correct service id
        if(!m_device.service_available(""))
            throw std::runtime_error {"Device does not support Media Renderer."};
    }

    bool set_media(const utils::media_data& data)
    {
        // TODO Use m_device.use_service(...) function here to play the media defined in data
        return m_device.use_service("", service_parameter {"", rapidxml::xml_document<char> {}});
    }

private:

    upnp_device& m_device;

};

} // namespace upnp

#endif
