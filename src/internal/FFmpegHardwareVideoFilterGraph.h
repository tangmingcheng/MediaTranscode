#pragma once

#include "internal/FFmpegHardwareTypes.h"
#include "internal/FFmpegRAII.h"

#include <string>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/buffer.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

    /*
     * Describes hardware filter graph policy.
     *
     * The builder is intentionally independent from the software VideoFilterGraph
     * so hwupload/hwdownload/scale decisions do not leak into the CPU frame path.
     */
    class HardwareVideoFilterGraphBuilder {
    public:
        struct Config {
            HardwareDeviceType deviceType = HardwareDeviceType::None;
            int outputWidth = 0;
            int outputHeight = 0;
            AVPixelFormat softwareFormat = AV_PIX_FMT_NONE;
            bool enableScale = false;
            bool keepFramesOnDevice = true;
        };

        static std::string buildDescription(const Config& config,
                                            std::string* error = nullptr);

        static bool supportsHardwareScale(HardwareDeviceType deviceType);
        static const char* scaleFilterName(HardwareDeviceType deviceType);
        static const char* softwarePixelFormatName(AVPixelFormat format);
    };

    /*
     * Real AVFilterGraph wrapper for hardware frames.
     *
     * Expected path:
     *   decoder hardware AVFrame -> buffersrc(hw_frames_ctx) -> hardware filters -> buffersink
     *
     * This class does not download frames to CPU unless keepFramesOnDevice is
     * explicitly set to false in the builder config.
     */
    class HardwareVideoFilterGraph {
    public:
        struct Config {
            AVBufferRef* inputHardwareFramesContext = nullptr;
            HardwareDeviceType deviceType = HardwareDeviceType::None;
            AVPixelFormat inputHardwarePixelFormat = AV_PIX_FMT_NONE;
            AVPixelFormat softwarePixelFormat = AV_PIX_FMT_NONE;
            int inputWidth = 0;
            int inputHeight = 0;
            int outputWidth = 0;
            int outputHeight = 0;
            AVRational inputTimeBase{ 0, 1 };
            AVRational inputFrameRate{ 0, 1 };
            bool enableScale = false;
            bool keepFramesOnDevice = true;
        };

        HardwareVideoFilterGraph() = default;
        ~HardwareVideoFilterGraph();

        HardwareVideoFilterGraph(const HardwareVideoFilterGraph&) = delete;
        HardwareVideoFilterGraph& operator=(const HardwareVideoFilterGraph&) = delete;

        HardwareVideoFilterGraph(HardwareVideoFilterGraph&& other) noexcept;
        HardwareVideoFilterGraph& operator=(HardwareVideoFilterGraph&& other) noexcept;

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
        static AVRational chooseInputFrameRate(AVRational inputFrameRate);

        FilterGraphPtr m_graph;
        AVFilterContext* m_bufferSrcCtx = nullptr;
        AVFilterContext* m_bufferSinkCtx = nullptr;
        AVRational m_inputFrameRate{ 0, 1 };
    };

} // namespace media::ffmpeg
