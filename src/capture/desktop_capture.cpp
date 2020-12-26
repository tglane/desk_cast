#include "capture/desktop_capture.hpp"

namespace capture
{

const char* runtime_error_msg = "Unable to initialize recorder";

size_t find_videostream_in_format(AVFormatContext* ctx)
{
    for(size_t i = 0; i < ctx->nb_streams; i++)
    {
        if(ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            return i;
    }
    throw std::runtime_error {runtime_error_msg};
}

} // namespace capture
