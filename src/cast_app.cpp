#include <cast_app.hpp>

#include <iostream>

#include <cast_device.hpp>

static const char* namespace_media = "urn:x-cast:com.google.cast.media";

namespace googlecast
{

cast_app::cast_app(cast_device* device, const std::string& app_id, const std::string& session_id,
    const std::string& transport_id, const json& nspaces)
    : m_device(device), m_app_id(app_id), m_session_id(session_id), m_transport_id(transport_id), m_namespaces(nspaces)
{}

bool cast_app::run() const
{
    uint64_t req_id = ++(m_device->m_request_id);
    json obj;
    obj["media"]["contentId"] = "https://kinsta.com/de/wp-content/uploads/sites/5/2019/09/jpg-vs-jpeg-1024x512.jpg";
    obj["media"]["contentType"] = "image/jpeg";
    obj["media"]["streamType"] = "NONE";
    // obj["media"]["contentId"] = "http://techslides.com/demos/sample-videos/small.mp4";
    // obj["media"]["contentType"] = "video/mp4";
    // obj["media"]["streamType"] = "BUFFERED";
    obj["type"] = "LOAD";
    obj["requestId"] = req_id;

    m_device->send_json(namespace_media, obj, m_transport_id);

    // Takes time to launch the app on older devices so lets wait a bit here
    json recv;
    uint8_t cnt = 0;
    while(true)
    {
        if(cnt >= 5)
            return false;

        recv = m_device->read(req_id);
        if(!recv.empty())
            break;

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "[APP RECEVICE]\n" << recv.dump(2) << std::endl;

    return true;
}

}; // namespace googlecast
