#pragma once

#include "internal/FFmpegHardwareDecoder.h"
#include "internal/FFmpegHardwareTypes.h"

#include <string>

extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg {

class FFmpegVideoFrameRoutingStrategy {
public:
    enum class Route {
        SoftwareFilter,
        HardwareTransferThenSoftwareFilter,
        HardwareZeroCopy,
        Invalid
    };

    struct Config {
        VideoExecutionMode executionMode = VideoExecutionMode::Cpu;
        bool zeroCopyPipeline = false;
        bool decoderUsesHardwareFrames = false;
        HardwareDecoderSupport::Config hardwareDecoderConfig;
    };

    struct Decision {
        Route route = Route::Invalid;
        bool expectedHardwareFrame = false;
        std::string error;
    };

    FFmpegVideoFrameRoutingStrategy() = default;

    void reset();
    bool initialize(const Config& config, std::string* error);

    Decision decide(const AVFrame* decodedFrame) const;
    bool isInitialized() const;

private:
    bool isExpectedHardwareFrame(const AVFrame* frame) const;

private:
    VideoExecutionMode m_executionMode = VideoExecutionMode::Cpu;
    bool m_zeroCopyPipeline = false;
    bool m_decoderUsesHardwareFrames = false;
    HardwareDecoderSupport::Config m_hardwareDecoderConfig;
    bool m_initialized = false;
};

} // namespace media::ffmpeg
