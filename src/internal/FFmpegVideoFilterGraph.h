#pragma once

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

    class VideoFilterGraph {
    public:
        struct Config {
            const AVCodecContext* decoderCtx = nullptr;
            const AVCodecContext* encoderCtx = nullptr;
            const AVStream* inputStream = nullptr;
            int outputFps = 25;
            bool enableConstantFps = false;
        };

        VideoFilterGraph() = default;
        ~VideoFilterGraph();

        VideoFilterGraph(const VideoFilterGraph&) = delete;
        VideoFilterGraph& operator=(const VideoFilterGraph&) = delete;

        VideoFilterGraph(VideoFilterGraph&& other) noexcept;
        VideoFilterGraph& operator=(VideoFilterGraph&& other) noexcept;

        void reset();

        bool initialize(const Config& config, std::string* error);
        bool sendFrame(AVFrame* frame, std::string* error);
        bool flush(std::string* error);

        /*
         * 返回值：
         *  1：成功取出一帧。
         *  0：当前没有更多帧，可能是 EAGAIN 或 EOF。
         * -1：发生错误，error 中包含详细信息。
         */
        int receiveFrame(AVFrame* frame, std::string* error);

        bool isInitialized() const;
        AVRational sinkTimeBase() const;
        AVRational inputFrameRate() const;

    private:
        static AVRational chooseInputFrameRate(const AVStream* inputStream, int outputFps);
        static std::string buildDescription(const AVCodecContext* encoderCtx,
                                            int outputFps,
                                            bool enableConstantFps);

        AVFilterGraph* m_graph = nullptr;
        AVFilterContext* m_bufferSrcCtx = nullptr;
        AVFilterContext* m_bufferSinkCtx = nullptr;
        AVRational m_inputFrameRate{ 0, 1 };
    };

} // namespace media::ffmpeg
