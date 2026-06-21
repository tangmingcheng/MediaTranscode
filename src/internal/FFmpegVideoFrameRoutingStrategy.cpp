#include "internal/FFmpegVideoFrameRoutingStrategy.h"

#include "internal/FFmpegUtils.h"

#include <sstream>

extern "C" {
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

const char* pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? name : "none";
}

} // namespace

void FFmpegVideoFrameRoutingStrategy::reset()
{
    m_executionMode = VideoExecutionMode::Cpu;
    m_zeroCopyPipeline = false;
    m_decoderUsesHardwareFrames = false;
    m_hardwareDecoderConfig = HardwareDecoderSupport::Config{};
    m_initialized = false;
}

bool FFmpegVideoFrameRoutingStrategy::initialize(const Config& config, std::string* error)
{
    reset();

    if (config.zeroCopyPipeline && config.executionMode != VideoExecutionMode::HardwareZeroCopy) {
        if (error) {
            *error = "frame routing strategy initialize failed: zero-copy pipeline requires HardwareZeroCopy execution mode";
        }
        return false;
    }

    if (config.executionMode != VideoExecutionMode::Cpu &&
        config.decoderUsesHardwareFrames &&
        !config.hardwareDecoderConfig.valid) {
        if (error) {
            *error = "frame routing strategy initialize failed: hardware execution has no valid decoder config";
        }
        return false;
    }

    m_executionMode = config.executionMode;
    m_zeroCopyPipeline = config.zeroCopyPipeline;
    m_decoderUsesHardwareFrames = config.decoderUsesHardwareFrames;
    m_hardwareDecoderConfig = config.hardwareDecoderConfig;
    m_initialized = true;

    return true;
}

FFmpegVideoFrameRoutingStrategy::Decision FFmpegVideoFrameRoutingStrategy::decide(
    const AVFrame* decodedFrame) const
{
    Decision decision;

    if (!m_initialized) {
        decision.error = "frame routing strategy is not initialized";
        return decision;
    }

    if (!decodedFrame) {
        decision.error = "frame routing strategy failed: decoded frame is null";
        return decision;
    }

    decision.expectedHardwareFrame = isExpectedHardwareFrame(decodedFrame);

    if (m_zeroCopyPipeline) {
        if (!decision.expectedHardwareFrame) {
            std::ostringstream oss;
            oss << "zero-copy pipeline expected hardware frame fmt="
                << pixelFormatName(m_hardwareDecoderConfig.hardwarePixelFormat)
                << " but decoder returned fmt="
                << pixelFormatName(static_cast<AVPixelFormat>(decodedFrame->format));
            decision.error = oss.str();
            return decision;
        }

        decision.route = Route::HardwareZeroCopy;
        return decision;
    }

    if (decision.expectedHardwareFrame) {
        decision.route = Route::HardwareTransferThenSoftwareFilter;
        return decision;
    }

    decision.route = Route::SoftwareFilter;
    return decision;
}

bool FFmpegVideoFrameRoutingStrategy::isInitialized() const
{
    return m_initialized;
}

bool FFmpegVideoFrameRoutingStrategy::isExpectedHardwareFrame(const AVFrame* frame) const
{
    return frame &&
        m_decoderUsesHardwareFrames &&
        m_hardwareDecoderConfig.valid &&
        frame->format == m_hardwareDecoderConfig.hardwarePixelFormat;
}

} // namespace media::ffmpeg
