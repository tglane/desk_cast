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

const char* outfile_path = "./test_data/capture.mp4";
const char* runtime_error_msg = "";

static int32_t find_video_stream(AVFormatContext* ctx)
{
    for(size_t i = 0; i < ctx->nb_streams; i++)
    {
        // grab_format_ctx->streams[i]->codecpar->codec_id
        if(ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            return i;
    }
    
    throw std::runtime_error {runtime_error_msg};
}

void desktop_capture()
{
    avdevice_register_all();
    avcodec_register_all();
    avdevice_register_all();


    // Init capture
    AVFormatContext* format_ctx = avformat_alloc_context();
    AVInputFormat* in_format = av_find_input_format("x11grab");
    if(avformat_open_input(&format_ctx, ":0.0+10,250", in_format, nullptr) != 0)
        throw std::runtime_error {runtime_error_msg};

    AVDictionary* options;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "preset", "medium", 0);

    // Find video stream in context
    int32_t video_idx = find_video_stream(format_ctx);
    if(video_idx == -1)
        throw std::runtime_error {runtime_error_msg};
    AVCodecContext* codec_ctx = format_ctx->streams[video_idx]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);    
    if(codec == nullptr || avcodec_open2(codec_ctx, codec, nullptr) < 0)
        throw std::runtime_error {runtime_error_msg};

    // Init output
    AVFormatContext* out_format_ctx;
    avformat_alloc_output_context2(&out_format_ctx, nullptr, nullptr, outfile_path);
    AVOutputFormat* out_format = av_guess_format(nullptr, outfile_path, nullptr);
    if(!out_format_ctx || !out_format)
        throw std::runtime_error {runtime_error_msg};

    AVStream* vid_stream = avformat_new_stream(out_format_ctx, nullptr);
    AVCodec* out_codec;
    AVCodecContext* out_codec_ctx = avcodec_alloc_context3(out_codec);
    if(!out_format_ctx)
        throw std::runtime_error {runtime_error_msg};

    // Set properties of the out video file
    // TODO find correct out file settings
    out_codec_ctx = vid_stream->codec;
    out_codec_ctx->codec_id = AV_CODEC_ID_MPEG4;
    out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    out_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    out_codec_ctx->bit_rate = 400000;
    out_codec_ctx->width = 2560;
    out_codec_ctx->height = 1440;
    out_codec_ctx->gop_size = 3;
    out_codec_ctx->max_b_frames = 1;
    out_codec_ctx->time_base.num = 1;
    out_codec_ctx->time_base.den = 30; // 15fps


    while(true)
    {

    }
}

