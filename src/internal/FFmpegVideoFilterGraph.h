#pragma once

#include "internal/FFmpegRAII.h"

#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

    class VideoFilterGraph {
    public:
        struct Config {
            const AVCodecContext* encoderCtx = nullptr;

            AVPixelFormat inputPixelFormat = AV_PIX_FMT_NONE;
            int inputWidth = 0;
            int inputHeight = 0;
            AVRational inputSampleAspectRatio{ 1, 1 };
            AVRational inputTimeBase{ 0, 1 };
            AVRational inputFrameRate{ 0, 1 };

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
        static AVRational chooseInputFrameRate(AVRational inputFrameRate, int outputFps);
        static std::string buildDescription(const AVCodecContext* encoderCtx,
                                            int outputFps,
                                            bool enableConstantFps);

        FilterGraphPtr m_graph;
        AVFilterContext* m_bufferSrcCtx = nullptr;
        AVFilterContext* m_bufferSinkCtx = nullptr;
        AVRational m_inputFrameRate{ 0, 1 };
    };

} // namespace media::ffmpeg
