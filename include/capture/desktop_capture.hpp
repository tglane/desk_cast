#ifndef DESKTOP_CAPTURE_HPP
#define DESKTOP_CAPTURE_HPP

#include <string>

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
    #include <libswscale/swscale.h>
}

namespace capture
{

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

} // namespace capture

#endif
