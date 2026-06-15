#include "internal/FFmpegVideoDecoderStage.h"

#include "internal/FFmpegUtils.h"

extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg {

FFmpegVideoDecoderStage::~FFmpegVideoDecoderStage()
{
    reset();
}

void FFmpegVideoDecoderStage::reset()
{
    m_hardwareDeviceContext.reset();

    if (m_decoderCtx) {
        avcodec_free_context(&m_decoderCtx);
    }

    m_inputStream = nullptr;
    m_hardwareDecoderConfig = HardwareDecoderSupport::Config{};
    m_hasHardwarePlan = false;
    m_hardwareDeviceAttached = false;
    m_usesHardwareFrames = false;
}

bool FFmpegVideoDecoderStage::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.inputStream) {
        if (error) {
            *error = "FFmpegVideoDecoderStage initialize failed: inputStream is null";
        }
        return false;
    }

    m_inputStream = config.inputStream;

    if (config.hardwarePlan &&
        config.hardwarePlan->valid &&
        config.hardwarePlan->executionMode != VideoExecutionMode::Cpu) {
        m_hasHardwarePlan = true;
        m_hardwareDecoderConfig = config.hardwarePlan->decoderConfig;
    }

    const AVCodec* decoder = avcodec_find_decoder(m_inputStream->codecpar->codec_id);
    if (!decoder) {
        if (error) {
            *error = "avcodec_find_decoder failed: unsupported input video codec";
        }
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(decoder);
    if (!m_decoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 decoder failed";
        }
        return false;
    }

    int ret = avcodec_parameters_to_context(m_decoderCtx, m_inputStream->codecpar);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_to_context decoder failed: " + errorString(ret);
        }
        return false;
    }

    if (!initializeHardwareDevice(decoder, error)) {
        return false;
    }

    ret = avcodec_open2(m_decoderCtx, decoder, nullptr);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_open2 decoder failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

bool FFmpegVideoDecoderStage::initializeHardwareDevice(const AVCodec* decoder,
                                                       std::string* error)
{
    if (!m_hasHardwarePlan) {
        return true;
    }

    if (!m_hardwareDecoderConfig.valid) {
        if (error) {
            *error = "hardware decoder initialization failed: planner returned invalid decoder config";
        }
        return false;
    }

    std::string hardwareError;
    if (!m_hardwareDeviceContext.initialize(
            m_hardwareDecoderConfig.deviceType,
            decoder,
            nullptr,
            &hardwareError)) {
        if (error) {
            *error = hardwareError;
        }
        return false;
    }

    AVBufferRef* deviceRef = m_hardwareDeviceContext.ref();
    if (!deviceRef) {
        if (error) {
            *error = "hardware decoder initialization failed: unable to reference device context";
        }
        return false;
    }

    m_decoderCtx->hw_device_ctx = deviceRef;
    m_decoderCtx->opaque = this;
    m_decoderCtx->get_format = &FFmpegVideoDecoderStage::selectDecoderPixelFormat;

    m_hardwareDeviceAttached = true;
    m_usesHardwareFrames = true;
    return true;
}

bool FFmpegVideoDecoderStage::sendPacket(AVPacket* packet, std::string* error)
{
    if (!m_decoderCtx) {
        if (error) {
            *error = "FFmpegVideoDecoderStage sendPacket failed: decoder is not initialized";
        }
        return false;
    }

    const int ret = avcodec_send_packet(m_decoderCtx, packet);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_packet decoder failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

bool FFmpegVideoDecoderStage::sendFlush(std::string* error)
{
    if (!m_decoderCtx) {
        return true;
    }

    const int ret = avcodec_send_packet(m_decoderCtx, nullptr);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_send_packet decoder flush failed: " + errorString(ret);
        }
        return false;
    }

    return true;
}

int FFmpegVideoDecoderStage::receiveFrame(AVFrame* frame, std::string* error)
{
    if (!m_decoderCtx) {
        if (error) {
            *error = "FFmpegVideoDecoderStage receiveFrame failed: decoder is not initialized";
        }
        return -1;
    }

    if (!frame) {
        if (error) {
            *error = "FFmpegVideoDecoderStage receiveFrame failed: frame is null";
        }
        return -1;
    }

    const int ret = avcodec_receive_frame(m_decoderCtx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    }

    if (ret < 0) {
        if (error) {
            *error = "avcodec_receive_frame decoder failed: " + errorString(ret);
        }
        return -1;
    }

    return 1;
}

bool FFmpegVideoDecoderStage::isInitialized() const
{
    return m_decoderCtx != nullptr;
}

AVCodecContext* FFmpegVideoDecoderStage::context() const
{
    return m_decoderCtx;
}

bool FFmpegVideoDecoderStage::hasHardwarePlan() const
{
    return m_hasHardwarePlan;
}

bool FFmpegVideoDecoderStage::usesHardwareFrames() const
{
    return m_usesHardwareFrames;
}

bool FFmpegVideoDecoderStage::hardwareDeviceAttached() const
{
    return m_hardwareDeviceAttached;
}

const HardwareDecoderSupport::Config& FFmpegVideoDecoderStage::hardwareDecoderConfig() const
{
    return m_hardwareDecoderConfig;
}

const HardwareDeviceContext& FFmpegVideoDecoderStage::hardwareDeviceContext() const
{
    return m_hardwareDeviceContext;
}

AVPixelFormat FFmpegVideoDecoderStage::selectDecoderPixelFormat(
    AVCodecContext* ctx,
    const AVPixelFormat* formats)
{
    auto* self = ctx ? static_cast<FFmpegVideoDecoderStage*>(ctx->opaque) : nullptr;

    if (self && self->m_hardwareDecoderConfig.valid) {
        for (const AVPixelFormat* p = formats; p && *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == self->m_hardwareDecoderConfig.hardwarePixelFormat) {
                return *p;
            }
        }
    }

    return formats ? formats[0] : AV_PIX_FMT_NONE;
}

} // namespace media::ffmpeg
