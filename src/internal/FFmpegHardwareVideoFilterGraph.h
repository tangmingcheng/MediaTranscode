#pragma once

#include "internal/FFmpegHardwareVideoFilterPipeline.h"
#include "internal/FFmpegRAII.h"

#include <string>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/buffer.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

namespace media::ffmpeg {

    class HardwareVideoFilterGraphBuilder {
    public:
        struct Config {
            HardwareBackendProfile backend;
            HardwareDeviceType deviceType = HardwareDeviceType::None;
            int outputWidth = 0;
            int outputHeight = 0;
            AVPixelFormat softwareFormat = AV_PIX_FMT_NONE;
            int outputFps = 0;
            bool enableConstantFps = false;
            bool enableScale = false;
            bool enableFormatConversion = false;
            bool keepFramesOnDevice = true;
        };

        static std::string buildDescription(const Config& config,
                                            std::string* error = nullptr);

        static bool supportsHardwareScale(HardwareDeviceType deviceType);
        static const char* scaleFilterName(HardwareDeviceType deviceType);
        static const char* softwarePixelFormatName(AVPixelFormat format);
    };

    class HardwareVideoFilterGraph {
    public:
        struct Config {
            AVBufferRef* inputHardwareFramesContext = nullptr;
            HardwareBackendProfile backend;
            HardwareDeviceType deviceType = HardwareDeviceType::None;
            AVPixelFormat inputHardwarePixelFormat = AV_PIX_FMT_NONE;
            AVPixelFormat inputSoftwarePixelFormat = AV_PIX_FMT_NONE;
            AVPixelFormat softwarePixelFormat = AV_PIX_FMT_NONE;
            int inputWidth = 0;
            int inputHeight = 0;
            int outputWidth = 0;
            int outputHeight = 0;
            AVRational inputTimeBase{ 0, 1 };
            AVRational inputFrameRate{ 0, 1 };
            int outputFps = 0;
            bool enableConstantFps = false;
            bool enableScale = false;
            bool enableFormatConversion = false;
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

        int receiveFrame(AVFrame* frame, std::string* error);

        bool isInitialized() const;
        AVRational sinkTimeBase() const;
        AVRational inputFrameRate() const;

    private:
        static AVRational chooseInputFrameRate(AVRational inputFrameRate);
        static HardwareBackendProfile resolveBackend(const HardwareBackendProfile& backend,
                                                     HardwareDeviceType fallbackDeviceType);

        FilterGraphPtr m_graph;
        AVFilterContext* m_bufferSrcCtx = nullptr;
        AVFilterContext* m_bufferSinkCtx = nullptr;
        AVRational m_inputFrameRate{ 0, 1 };
    };

} // namespace media::ffmpeg
