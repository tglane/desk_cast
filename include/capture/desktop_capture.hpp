#ifndef DESKTOP_CAPTURE_HPP
#define DESKTOP_CAPTURE_HPP

#include <string>
#include <future>
#include <stdexcept>

#include <iostream>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
    #include <libswscale/swscale.h>

    #include <libavutil/imgutils.h>
}

namespace capture
{

extern const char* runtime_error_msg;

size_t find_videostream_in_format(AVFormatContext*);

template<AVCodecID CODEC_ID>
class recorder
{
public:
    recorder() = delete;
    recorder(const recorder&) = default;
    recorder& operator=(const recorder&) = default;
    recorder(recorder&&) = default;
    recorder& operator=(recorder&&) = default;

    recorder(const std::string& outfile_path);

    ~recorder();

    void start_recording();

    void stop_recording();

private:

    void init();

    void record_loop();

    std::string m_outfile_path;

    bool m_record_condition = false;
    std::future<void> m_loop;

    AVInputFormat* m_in_format = nullptr;
    AVOutputFormat* m_out_format = nullptr;

    AVCodecContext* m_codec_ctx = nullptr;
    AVFormatContext* m_format_ctx = nullptr;
    AVCodec* m_codec = nullptr;

    AVOutputFormat* out_format_hmm = nullptr;
    AVFormatContext* m_out_format_ctx = nullptr;
    AVCodecContext* m_out_codec_ctx = nullptr;
    AVCodec* m_out_codec = nullptr;

    AVStream* m_vid_stream = nullptr;

    int m_stream_idx;
};


template<AVCodecID CODEC_ID>
recorder<CODEC_ID>::recorder(const std::string& outfile_path)
    : m_outfile_path {outfile_path}
{
    avdevice_register_all();
    // avcodec_register_all();
    avdevice_register_all();

    init();
}

template<AVCodecID CODEC_ID>
recorder<CODEC_ID>::~recorder()
{
    if(m_record_condition)
        stop_recording();

    if(m_format_ctx)
    {
        avformat_close_input(&m_format_ctx);
        avformat_free_context(m_format_ctx);
    }

    if(m_out_format_ctx)
        avcodec_free_context(&m_out_codec_ctx);
}

template<AVCodecID CODEC_ID>
void recorder<CODEC_ID>::start_recording()
{
    m_record_condition = true;
    std::cout << "start record loop" << std::endl;
    m_loop = std::async(std::launch::async, &recorder<CODEC_ID>::record_loop, this);
}

template<AVCodecID CODEC_ID>
void recorder<CODEC_ID>::stop_recording()
{
    std::cout << "stop record loop" << std::endl;
    m_record_condition = false;
}

template<AVCodecID CODEC_ID>
void recorder<CODEC_ID>::init()
{
    m_format_ctx = avformat_alloc_context();
    m_in_format = av_find_input_format("x11grab");
    if(avformat_open_input(&m_format_ctx, ":0.0+10,250", m_in_format, nullptr) != 0)
        throw std::runtime_error {runtime_error_msg};

    AVDictionary* options = nullptr;
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "preset", "medium", 0);

    // Find video stream in context
    m_stream_idx = find_videostream_in_format(m_format_ctx);
    m_codec = avcodec_find_decoder(m_format_ctx->streams[m_stream_idx]->codecpar->codec_id);
    m_codec_ctx = avcodec_alloc_context3(m_codec);

    if(m_codec == nullptr)
        std::cout << "codec" << std::endl;
    if(m_codec_ctx == nullptr)
        std::cout << "ctx" << std::endl;

    if(m_codec == nullptr || avcodec_open2(m_codec_ctx, m_codec, nullptr) < 0)
        throw std::runtime_error {runtime_error_msg};

    std::cout << "Hello2" << std::endl;
    // Init output
    avformat_alloc_output_context2(&m_out_format_ctx, nullptr, nullptr, m_outfile_path.data());
    m_out_format = av_guess_format(nullptr, m_outfile_path.data(), nullptr);
    if(!m_out_format_ctx || !m_out_format)
        throw std::runtime_error {runtime_error_msg};

    std::cout << "Hello1" << std::endl;
    m_vid_stream = avformat_new_stream(m_out_format_ctx, nullptr);
    m_out_codec_ctx = avcodec_alloc_context3(m_out_codec);
    if(!m_out_format_ctx)
        throw std::runtime_error {runtime_error_msg};

    // Set properties of the out video file
    // TODO find correct out file settings
    m_out_codec_ctx = m_vid_stream->codec; // TODO ????
    m_out_codec_ctx->codec_id = CODEC_ID;
    m_out_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    m_out_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_out_codec_ctx->bit_rate = 400000;
    m_out_codec_ctx->width = 1920;
    m_out_codec_ctx->height = 1080;
    m_out_codec_ctx->gop_size = 3;
    m_out_codec_ctx->max_b_frames = 1;
    m_out_codec_ctx->time_base.num = 1;
    m_out_codec_ctx->time_base.den = 30; // 15fps

    if(CODEC_ID == AV_CODEC_ID_H264)
        av_opt_set(m_out_codec_ctx->priv_data, "preset", "slow", 0);

    m_out_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if(!m_out_codec)
        throw std::runtime_error {runtime_error_msg};

    // Some containers require global headers so mark the encoder to behave the correct way
    if(m_out_format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        m_out_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if(avcodec_open2(m_out_codec_ctx, m_out_codec, nullptr) < 0)
        throw std::runtime_error {runtime_error_msg};

    // Create empty video file
    if(!(m_out_format_ctx->flags & AVFMT_NOFILE))
    {
        if(avio_open2(&m_out_format_ctx->pb, m_outfile_path.data(), AVIO_FLAG_WRITE, nullptr, nullptr) < 0)
            throw std::runtime_error {runtime_error_msg};
    }
    if(!m_out_format_ctx->nb_streams)
        throw std::runtime_error {runtime_error_msg};

    if(avformat_write_header(m_out_format_ctx, &options) < 0)
        throw std::runtime_error {runtime_error_msg};

    // Dump information about output file
    av_dump_format(m_out_format_ctx, 0, m_outfile_path.data(), 1);

    av_dict_free(&options);
}

template<AVCodecID CODEC_ID>
void recorder<CODEC_ID>::record_loop()
{
    // TODO change function to create hls stream files and not one mpeg4 container

    AVPacket* packet = reinterpret_cast<AVPacket*>(av_malloc(sizeof(AVPacket)));
    av_init_packet(packet);
    AVFrame* frame = av_frame_alloc();
    AVFrame* out_frame = av_frame_alloc();

    size_t nbytes = av_image_get_buffer_size(m_out_codec_ctx->pix_fmt, m_out_codec_ctx->width, m_out_codec_ctx->height, 32);
    uint8_t* video_outbuf = reinterpret_cast<uint8_t*>(av_malloc(nbytes));

    if(av_image_fill_arrays(out_frame->data, out_frame->linesize, video_outbuf, AV_PIX_FMT_YUV420P, m_out_codec_ctx->width, m_out_codec_ctx->height, 1) < 0)
        return; // TODO free allocated memory in error case

    SwsContext* sws_ctx = sws_getCachedContext(nullptr, m_codec_ctx->width, m_codec_ctx->height, m_codec_ctx->pix_fmt,
        m_out_codec_ctx->width, m_out_codec_ctx->height, m_out_codec_ctx->pix_fmt, SWS_BICUBIC,
        nullptr, nullptr, nullptr);
    int frame_finished;
    while(m_record_condition && av_read_frame(m_format_ctx, packet) >= 0)
    {
        if(packet->stream_index != m_stream_idx)
            continue;

        avcodec_send_packet(m_codec_ctx, packet);
        if(avcodec_receive_frame(m_codec_ctx, frame) == 0)
        {
            AVPacket out_packet;
            sws_scale(sws_ctx, frame->data, frame->linesize, 0, m_codec_ctx->height, out_frame->data, out_frame->linesize);
            av_init_packet(&out_packet);
            out_packet.data = nullptr;
            out_packet.size = 0;

            avcodec_send_frame(m_out_codec_ctx, out_frame);
            if(avcodec_receive_packet(m_out_codec_ctx, &out_packet) == 0)
            {
                // if(out_packet.pts != AV_NOPTS_VALUE)
                //     out_packet.pts = av_rescale_q(out_packet.pts, m_vid_stream->codec->time_base, m_vid_stream->time_base);

                // if(out_packet.dts != AV_NOPTS_VALUE)
                //     out_packet.dts = av_rescale_q(out_packet.dts, m_vid_stream->codec->time_base, m_vid_stream->time_base);

                // if(av_write_frame(m_out_format_ctx, &out_packet) != 0)
                    std::cout << "ERROR WRITING FRAME" << std::endl;
            }
            av_packet_unref(&out_packet);
        }
    }

    av_write_trailer(m_out_format_ctx);    

    av_free(video_outbuf);
    av_frame_free(&out_frame);
    av_frame_free(&frame);
    av_free(packet);
}

} // namespace capture

#endif
