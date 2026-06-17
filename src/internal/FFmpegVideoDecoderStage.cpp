#include "internal/FFmpegVideoDecoderStage.h"

#include "internal/FFmpegError.h"
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
    m_decoderCtx.reset();

    m_inputStream = nullptr;
    m_hardwareDecoderConfig = HardwareDecoderSupport::Config{};
    m_hasHardwarePlan = false;
    m_hardwareDeviceAttached = false;
    m_usesHardwareFrames = false;
}

Status FFmpegVideoDecoderStage::initialize(const Config& config)
{
    reset();

    if (!config.inputStream) {
        return Status::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoDecoderStage initialize failed: inputStream is null"));
    }

    m_inputStream = config.inputStream;

    if (config.hardwarePlan &&
        config.hardwarePlan->valid &&
        config.hardwarePlan->executionMode != VideoExecutionMode::Cpu) {
        m_hasHardwarePlan = true;
        m_hardwareDecoderConfig = config.hardwarePlan->decoderConfig;
    }

    const AVCodec* decoder = m_hasHardwarePlan && m_hardwareDecoderConfig.decoder
        ? m_hardwareDecoderConfig.decoder
        : avcodec_find_decoder(m_inputStream->codecpar->codec_id);

    if (!decoder) {
        return Status::failure(ErrorInfo::unsupported(
            "avcodec_find_decoder failed: unsupported input video codec"));
    }

    m_decoderCtx = makeCodecContext(decoder);
    if (!m_decoderCtx) {
        return Status::failure(makeAllocationError(
            "avcodec_alloc_context3 decoder failed"));
    }

    int ret = avcodec_parameters_to_context(m_decoderCtx.get(), m_inputStream->codecpar);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_parameters_to_context decoder failed", ret));
    }

    Status hardwareStatus = initializeHardwareDevice(decoder);
    if (!hardwareStatus) {
        return hardwareStatus;
    }

    ret = avcodec_open2(m_decoderCtx.get(), decoder, nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_open2 decoder failed [" +
                std::string(decoder->name ? decoder->name : "unknown") + "]",
            ret));
    }

    return Status::success();
}

Status FFmpegVideoDecoderStage::initializeHardwareDevice(const AVCodec* decoder)
{
    if (!m_hasHardwarePlan) {
        return Status::success();
    }

    if (!m_hardwareDecoderConfig.valid) {
        return Status::failure(makeError(
            ErrorCode::HardwareUnavailable,
            "hardware decoder initialization failed: planner returned invalid decoder config"));
    }

    if (!m_hardwareDecoderConfig.requiresHardwareDeviceContext) {
        m_decoderCtx->opaque = this;
        m_decoderCtx->get_format = &FFmpegVideoDecoderStage::selectDecoderPixelFormat;
        m_hardwareDeviceAttached = false;
        m_usesHardwareFrames = true;
        return Status::success();
    }

    std::string hardwareError;
    if (!m_hardwareDeviceContext.initialize(
            m_hardwareDecoderConfig.deviceType,
            decoder,
            nullptr,
            &hardwareError)) {
        return Status::failure(makeLegacyError(
            hardwareError,
            ErrorCode::HardwareUnavailable));
    }

    BufferRefPtr deviceRef = m_hardwareDeviceContext.ref();
    if (!deviceRef) {
        return Status::failure(makeError(
            ErrorCode::HardwareUnavailable,
            "hardware decoder initialization failed: unable to reference device context"));
    }

    m_decoderCtx->hw_device_ctx = deviceRef.release();
    m_decoderCtx->opaque = this;
    m_decoderCtx->get_format = &FFmpegVideoDecoderStage::selectDecoderPixelFormat;

    m_hardwareDeviceAttached = true;
    m_usesHardwareFrames = true;
    return Status::success();
}

Status FFmpegVideoDecoderStage::sendPacket(AVPacket* packet)
{
    if (!m_decoderCtx) {
        return Status::failure(ErrorInfo::notInitialized(
            "FFmpegVideoDecoderStage sendPacket failed: decoder is not initialized"));
    }

    const int ret = avcodec_send_packet(m_decoderCtx.get(), packet);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_packet decoder failed", ret));
    }

    return Status::success();
}

Status FFmpegVideoDecoderStage::sendFlush()
{
    if (!m_decoderCtx) {
        return Status::success();
    }

    const int ret = avcodec_send_packet(m_decoderCtx.get(), nullptr);
    if (ret < 0) {
        return Status::failure(makeFFmpegError(
            "avcodec_send_packet decoder flush failed", ret));
    }

    return Status::success();
}

Result<FFmpegVideoDecoderStage::ReceiveFrameState> FFmpegVideoDecoderStage::receiveFrame(AVFrame* frame)
{
    if (!m_decoderCtx) {
        return Result<ReceiveFrameState>::failure(ErrorInfo::notInitialized(
            "FFmpegVideoDecoderStage receiveFrame failed: decoder is not initialized"));
    }

    if (!frame) {
        return Result<ReceiveFrameState>::failure(ErrorInfo::invalidArgument(
            "FFmpegVideoDecoderStage receiveFrame failed: frame is null"));
    }

    const int ret = avcodec_receive_frame(m_decoderCtx.get(), frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return Result<ReceiveFrameState>::success(ReceiveFrameState::NeedMoreInput);
    }

    if (ret < 0) {
        return Result<ReceiveFrameState>::failure(makeFFmpegError(
            "avcodec_receive_frame decoder failed", ret));
    }

    return Result<ReceiveFrameState>::success(ReceiveFrameState::Frame);
}

bool FFmpegVideoDecoderStage::isInitialized() const
{
    return m_decoderCtx != nullptr;
}

AVCodecContext* FFmpegVideoDecoderStage::context() const
{
    return m_decoderCtx.get();
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

} // namespace media::ffmpeg
