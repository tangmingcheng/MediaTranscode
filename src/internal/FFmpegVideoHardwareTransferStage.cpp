#include "internal/FFmpegVideoHardwareTransferStage.h"

#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

const char* pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? name : "none";
}

bool shouldLogHardwareTransfer(int64_t frameCount)
{
    return frameCount <= 3 || frameCount % 120 == 0;
}

} // namespace

FFmpegVideoHardwareTransferStage::~FFmpegVideoHardwareTransferStage()
{
    reset();
}

void FFmpegVideoHardwareTransferStage::reset()
{
    m_zeroCopyPipeline = false;
    m_initialized = false;
    m_transferLogCount = 0;
}

bool FFmpegVideoHardwareTransferStage::initialize(const Config& config, std::string* error)
{
    reset();

    m_zeroCopyPipeline = config.zeroCopyPipeline;
    m_initialized = true;

    return true;
}

bool FFmpegVideoHardwareTransferStage::transferToSoftware(
    AVFrame* hardwareFrame,
    AVFrame* softwareFrame,
    std::string* error) const
{
    if (!m_initialized) {
        if (error) {
            *error = "hardware frame transfer failed: transfer stage is not initialized";
        }
        return false;
    }

    if (m_zeroCopyPipeline) {
        if (error) {
            *error = "zero-copy pipeline violation: attempted hardware-to-software transfer";
        }
        return false;
    }

    if (!hardwareFrame || !softwareFrame) {
        if (error) {
            *error = "hardware frame transfer failed: frame is null";
        }
        return false;
    }

    av_frame_unref(softwareFrame);

    const int ret = av_hwframe_transfer_data(softwareFrame, hardwareFrame, 0);
    if (ret < 0) {
        if (error) {
            *error = "av_hwframe_transfer_data decoder frame failed: " + errorString(ret);
        }
        return false;
    }

    softwareFrame->pts = hardwareFrame->pts;
    softwareFrame->pkt_dts = hardwareFrame->pkt_dts;
    softwareFrame->sample_aspect_ratio = hardwareFrame->sample_aspect_ratio;

    ++m_transferLogCount;
    if (shouldLogHardwareTransfer(m_transferLogCount)) {
        spdlog::warn(
            "[ZC][CPU_TRANSFER] frame={} hw_fmt={}, sw_fmt={}, sw_size={}x{}",
            m_transferLogCount,
            pixelFormatName(static_cast<AVPixelFormat>(hardwareFrame->format)),
            pixelFormatName(static_cast<AVPixelFormat>(softwareFrame->format)),
            softwareFrame->width,
            softwareFrame->height
        );
    }

    return true;
}

bool FFmpegVideoHardwareTransferStage::isInitialized() const
{
    return m_initialized;
}

} // namespace media::ffmpeg
