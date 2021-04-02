#ifndef DEFAULT_MEDIA_RECEIVER_HPP
#define DEFAULT_MEDIA_RECEIVER_HPP

#include <cassert>

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
        : m_status {dmr_status::idle}, m_device {static_cast<cast_device&&>(device)}
    {
        // TODO Make sure the default media receiver is supported by the given device
        // TODO Maybe launch the dmr but without any content?
    }

    bool set_media(const media_data& data)
    {
        assert(m_status != dmr_status::closed);

        json payload;
        payload["media"]["contentId"] = data.url;
        payload["media"]["contentType"] = data.mime_type;
        payload["media"]["streamType"] = "LIVE"; // TODO Figure out how to set this correct for all content types
        payload["type"] = "LOAD";

        return m_device.launch_app("CC1AD845", static_cast<json&&>(payload));
    }

    cast_device get_device()
    {
        // TODO I dont know if this is useful
        m_status = dmr_status::closed;
        return static_cast<cast_device&&>(m_device);
    }

private:

    dmr_status m_status;

    cast_device m_device;

};

} // namespace googlecast

#endif

