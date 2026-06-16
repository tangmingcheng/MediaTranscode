#include "internal/FFmpegVideoHardwareTransferStage.h"

#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <atomic>

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

std::atomic_bool g_hardwareTransferLogged{ false };

} // namespace

FFmpegVideoHardwareTransferStage::~FFmpegVideoHardwareTransferStage()
{
    reset();
}

void FFmpegVideoHardwareTransferStage::reset()
{
    m_decoderCtx = nullptr;
    m_zeroCopyPipeline = false;
    m_initialized = false;
}

bool FFmpegVideoHardwareTransferStage::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.decoderCtx) {
        if (error) {
            *error = "FFmpegVideoHardwareTransferStage initialize failed: decoderCtx is null";
        }
        return false;
    }

    m_decoderCtx = config.decoderCtx;
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

    bool expected = false;
    if (g_hardwareTransferLogged.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
        spdlog::warn(
            "[ZC][CPU_TRANSFER] hardware-to-software transfer enabled: hw_fmt={}, sw_fmt={}",
            pixelFormatName(static_cast<AVPixelFormat>(hardwareFrame->format)),
            pixelFormatName(m_decoderCtx ? m_decoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE)
        );
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

    return true;
}

bool FFmpegVideoHardwareTransferStage::isInitialized() const
{
    return m_initialized;
}

} // namespace media::ffmpeg
