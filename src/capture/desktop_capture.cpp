// #include <capture/desktop_capture.hpp>

#include <memory>
#include <stdexcept>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
    #include <libswscale/swscale.h>
}

const char* runtime_error_msg = "";

void desktop_capture()
{
    avdevice_register_all();
    AVInputFormat* grab_format = av_find_input_format("x11grab"); // Do i have to delete this?
    // AVFormatContext* grab_format_ctx = avformat_alloc_context();
    std::unique_ptr<AVFormatContext, void(*)(AVFormatContext*)> grab_format_ctx {avformat_alloc_context(), [](AVFormatContext* ctx) { 
        avformat_close_input(&ctx); 
    }};

    AVDictionary* options_ptr = nullptr;
    // TODO Set options

    AVFormatContext* format_ctx_ptr = grab_format_ctx.get();
    // Check what to put in the url string ":0.0+10,20"
    if(avformat_open_input(&format_ctx_ptr, ":0.0+10,20", grab_format, &options_ptr) != 0)
    {
        av_dict_free(&options_ptr);
        throw std::runtime_error{runtime_error_msg};
    }
    av_dict_free(&options_ptr);

    if(avformat_find_stream_info(grab_format_ctx.get(), nullptr) < 0)
        throw std::runtime_error {runtime_error_msg};

    // Find video stream in context
    for(int32_t i = 0; i < grab_format_ctx->nb_streams; i++)
    {
        // AVCodec* codec = avcodec_find_decoder(grab_format_ctx->streams[i]->codecpar->codec_id);

        if(grab_format_ctx->streams[i]->codecpar->codec_id == AVMEDIA_TYPE_VIDEO)
        {
            // Found video stream
        }
    }
}

