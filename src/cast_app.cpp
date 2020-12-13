#include <cast_app.hpp>

#include <iostream>

#include <cast_device.hpp>
#include <utils.hpp>

static const char* namespace_media = "urn:x-cast:com.google.cast.media";

namespace googlecast
{

cast_app::cast_app(cast_device* device, const std::string& app_id, const std::string& session_id,
    const std::string& transport_id, const json& nspaces)
    : m_device {device}, m_app_id {app_id}, m_session_id {session_id}, m_transport_id {transport_id}, m_namespaces {nspaces}
{}

bool cast_app::set_media() const
{
    std::string content_id;
    try {
        content_id = "https://" + utils::get_local_ipaddr() + ":5770/test_video.mp4";
    } catch(std::runtime_error& e) {
        return false;
    }

    uint64_t req_id = ++(m_device->m_request_id);
    json obj;
    // --- image ---
    // obj["media"]["contentId"] = "https://kinsta.com/de/wp-content/uploads/sites/5/2019/09/jpg-vs-jpeg-1024x512.jpg";
    // obj["media"]["contentId"] = content_id;
    // obj["media"]["contentType"] = "image/jpeg";
    // obj["media"]["streamType"] = "NONE";
    // --- static mp4 video ---
    // obj["media"]["contentId"] = "http://techslides.com/demos/sample-videos/small.mp4";
    obj["media"]["contentId"] = content_id;
    obj["media"]["contentType"] = "video/mp4";
    obj["media"]["streamType"] = "BUFFERED";
    // --- hls streamed video ---
    // obj["media"]["contentId"] = content_id;
    // obj["media"]["contentType"] = "application/x-mpegurl";
    // obj["media"]["streamType"] = "LIVE";
    // -------------
    obj["type"] = "LOAD";
    obj["requestId"] = req_id;

    m_device->send_json(namespace_media, obj, m_transport_id);

    // Takes time to launch the app on older devices so lets wait a bit here
    json recv;
    uint8_t cnt = 0;
    while(true)
    {
        if(cnt >= 10)
            return false;

        recv = m_device->read(req_id);
        if(!recv.empty())
            break;

        cnt++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "[DEBUG] Castdevice launch-response:\n" << recv.dump() << std::endl;

    // Check if response from device indicates loading error or not
    if(recv.contains("type") && recv["type"] == "MEDIA_STATUS")
        return true;
    else
        return false;
}

}; // namespace googlecast
