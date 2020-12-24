#ifndef DESKTOP_CAPTURE_HPP
#define DESKTOP_CAPTURE_HPP

#include <string>
#include <stdexcept>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
    #include <libswscale/swscale.h>
}

namespace capture
{

    extern const char* runtime_error_msg;
    size_t find_video_stream(AVFormatContext*);

    template<AVCodecID CODEC_ID>
    class recorder
    {
    public:

        recorder(const std::string& outfile_path);

        ~recorder();

        void start_recording() const;

        void stop_recording() const;

    private:

        void init();

        std::string m_outfile_path;

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

    };


template<AVCodecID CODEC_ID>
recorder<CODEC_ID>::recorder(const std::string& outfile_path)
    : m_outfile_path {outfile_path}
{
    avdevice_register_all();
    avcodec_register_all();
    avdevice_register_all();

    init();
}

template<AVCodecID CODEC_ID>
recorder<CODEC_ID>::~recorder()
{
    if(m_format_ctx)
    {
        avformat_close_input(&m_format_ctx);
        avformat_free_context(m_format_ctx);
    }

    if(m_out_format_ctx)
        avcodec_free_context(&m_out_codec_ctx);
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
    size_t video_idx = find_video_stream(m_format_ctx);
    m_codec_ctx = m_format_ctx->streams[video_idx]->codec;
    m_codec = avcodec_find_decoder(m_codec_ctx->codec_id); // deprecated ...
    if(m_codec == nullptr || avcodec_open2(m_codec_ctx, m_codec, nullptr) < 0)
        throw std::runtime_error {runtime_error_msg};

    // Init output
    m_out_format_ctx;
    avformat_alloc_output_context2(&m_out_format_ctx, nullptr, nullptr, m_outfile_path.data());
    m_out_format = av_guess_format(nullptr, m_outfile_path.data(), nullptr);
    if(!m_out_format_ctx || !m_out_format)
        throw std::runtime_error {runtime_error_msg};

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
    m_out_codec_ctx->width = 2560;
    m_out_codec_ctx->height = 1440;
    m_out_codec_ctx->gop_size = 3;
    m_out_codec_ctx->max_b_frames = 1;
    m_out_codec_ctx->time_base.num = 1;
    m_out_codec_ctx->time_base.den = 30; // 15fps

    if(m_out_codec_ctx->codec_id == AV_CODEC_ID_H264)
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
void recorder<CODEC_ID>::start_recording() const
{
    // TODO
}

template<AVCodecID CODEC_ID>
void recorder<CODEC_ID>::stop_recording() const
{
    // TODO
}

} // namespace capture

#endif
