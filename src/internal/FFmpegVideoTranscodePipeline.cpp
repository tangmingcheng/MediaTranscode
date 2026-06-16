#include "internal/FFmpegVideoTranscodePipeline.h"

#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <atomic>
#include <sstream>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mathematics.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

AVPixelFormat expectedSoftwareFormatAfterHardwareDownload(HardwareDeviceType deviceType,
                                                         AVPixelFormat decoderSoftwareFormat)
{
    switch (deviceType) {
    case HardwareDeviceType::D3D11VA:
    case HardwareDeviceType::CUDA:
    case HardwareDeviceType::QSV:
    case HardwareDeviceType::VAAPI:
    case HardwareDeviceType::DRM:
        if (decoderSoftwareFormat == AV_PIX_FMT_NONE ||
            decoderSoftwareFormat == AV_PIX_FMT_YUV420P) {
            return AV_PIX_FMT_NV12;
        }
        return decoderSoftwareFormat;

    case HardwareDeviceType::VideoToolbox:
        if (decoderSoftwareFormat == AV_PIX_FMT_NONE) {
            return AV_PIX_FMT_NV12;
        }
        return decoderSoftwareFormat;

    case HardwareDeviceType::Auto:
    case HardwareDeviceType::None:
    default:
        return decoderSoftwareFormat;
    }
}

const char* pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? name : "none";
}

std::atomic_bool g_hardwareTransferLogged{ false };

} // namespace

FFmpegVideoTranscodePipeline::~FFmpegVideoTranscodePipeline()
{
    reset();
}

void FFmpegVideoTranscodePipeline::reset()
{
    m_hardwareFilterGraph.reset();
    m_filterGraph.reset();
    m_packetWriter.reset();
    m_encoderStage.reset();
    m_decoderStage.reset();

    if (m_softwareTransferFrame) {
        av_frame_free(&m_softwareTransferFrame);
    }

    if (m_filteredFrame) {
        av_frame_free(&m_filteredFrame);
    }

    if (m_decodedFrame) {
        av_frame_free(&m_decodedFrame);
    }

    m_config = TranscodeConfig{};
    m_inputVideoStream = nullptr;
    m_outputFmtCtx = nullptr;
    m_outputVideoStream = nullptr;
    m_timeline = nullptr;
    m_lastSubmittedPts = AV_NOPTS_VALUE;
    m_packetCount = 0;
    m_lastWrittenOutTimeMs = 0;
    m_outputFps = 0;
    m_enableConstantFps = false;
    m_hardwarePlan = HardwarePipelinePlan{};
    m_hasHardwarePlan = false;
    m_hardwareBackend = HardwareBackendProfile{};
    m_zeroCopyPipeline = false;
    m_hardwareFilterGraphInitialized = false;
}

bool FFmpegVideoTranscodePipeline::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.transcodeConfig) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: transcodeConfig is null";
        }
        return false;
    }

    if (!config.inputVideoStream) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: inputVideoStream is null";
        }
        return false;
    }

    if (!config.outputFmtCtx) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: outputFmtCtx is null";
        }
        return false;
    }

    if (!config.timeline) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline initialize failed: timeline is null";
        }
        return false;
    }

    m_config = *config.transcodeConfig;
    m_inputVideoStream = config.inputVideoStream;
    m_outputFmtCtx = config.outputFmtCtx;
    m_timeline = config.timeline;

    if (config.hardwarePlan &&
        config.hardwarePlan->valid &&
        config.hardwarePlan->executionMode != VideoExecutionMode::Cpu) {
        m_hardwarePlan = *config.hardwarePlan;
        m_hasHardwarePlan = true;
    }

    return openDecoder(error) &&
        openEncoder(error) &&
        initializeFilterGraph(error) &&
        initializePacketWriter(error) &&
        allocateFrames(error);
}

bool FFmpegVideoTranscodePipeline::openDecoder(std::string* error)
{
    FFmpegVideoDecoderStage::Config config;
    config.inputStream = m_inputVideoStream;
    config.hardwarePlan = m_hasHardwarePlan ? &m_hardwarePlan : nullptr;
    return m_decoderStage.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::openEncoder(std::string* error)
{
    if (!m_decoderStage.context()) {
        if (error) {
            *error = "open encoder failed: decoder stage is not initialized";
        }
        return false;
    }

    FFmpegVideoEncoderStage::Config config;
    config.transcodeConfig = &m_config;
    config.hardwarePlan = m_hasHardwarePlan ? &m_hardwarePlan : nullptr;
    config.decoderCtx = m_decoderStage.context();
    config.inputVideoStream = m_inputVideoStream;
    config.outputFmtCtx = m_outputFmtCtx;
    config.hardwareDeviceContext = &m_decoderStage.hardwareDeviceContext();
    config.decoderUsesHardwareFrames = m_decoderStage.usesHardwareFrames();
    config.decoderHardwareDeviceAttached = m_decoderStage.hardwareDeviceAttached();

    if (!m_encoderStage.initialize(config, error)) {
        return false;
    }

    m_outputVideoStream = m_encoderStage.outputStream();
    m_outputFps = m_encoderStage.outputFps();
    m_enableConstantFps = m_encoderStage.enableConstantFps();
    m_hardwareBackend = m_encoderStage.hardwareBackend();
    m_zeroCopyPipeline = m_encoderStage.zeroCopyPipeline();

    return true;
}

bool FFmpegVideoTranscodePipeline::initializeFilterGraph(std::string* error)
{
    if (m_zeroCopyPipeline) {
        return true;
    }

    return initializeSoftwareFilterGraph(error);
}

bool FFmpegVideoTranscodePipeline::initializeSoftwareFilterGraph(std::string* error)
{
    AVCodecContext* encoderCtx = encoderContext();
    if (!encoderCtx) {
        if (error) {
            *error = "initialize software filter graph failed: encoder stage is not initialized";
        }
        return false;
    }

    VideoFilterGraph::Config config;
    config.decoderCtx = m_decoderStage.context();
    config.encoderCtx = encoderCtx;
    config.inputStream = m_inputVideoStream;
    config.outputFps = m_outputFps;
    config.enableConstantFps = m_enableConstantFps;

    if (m_decoderStage.usesHardwareFrames()) {
        AVCodecContext* decoderCtx = m_decoderStage.context();
        config.inputPixelFormat = expectedSoftwareFormatAfterHardwareDownload(
            m_decoderStage.hardwareDeviceContext().resolvedDeviceType(),
            decoderCtx ? decoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE
        );
    }

    return m_filterGraph.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::initializeHardwareFilterGraphFromFrame(
    const AVFrame* frame,
    std::string* error)
{
    if (!frame) {
        if (error) {
            *error = "initialize hardware filter graph failed: frame is null";
        }
        return false;
    }

    if (!frame->hw_frames_ctx) {
        if (error) {
            *error = "initialize hardware filter graph failed: frame has no hw_frames_ctx";
        }
        return false;
    }

    AVCodecContext* encoderCtx = encoderContext();
    if (!encoderCtx) {
        if (error) {
            *error = "initialize hardware filter graph failed: encoder stage is not initialized";
        }
        return false;
    }

    HardwareVideoFilterGraph::Config config;
    config.inputStream = m_inputVideoStream;
    config.inputHardwareFramesContext = frame->hw_frames_ctx;
    config.deviceType = m_hardwareBackend.deviceType;
    config.inputHardwarePixelFormat = static_cast<AVPixelFormat>(frame->format);
    config.softwarePixelFormat = encoderCtx->pix_fmt;
    config.inputWidth = frame->width;
    config.inputHeight = frame->height;
    config.outputWidth = encoderCtx->width;
    config.outputHeight = encoderCtx->height;
    config.enableScale = config.outputWidth > 0 &&
        config.outputHeight > 0 &&
        (config.outputWidth != config.inputWidth || config.outputHeight != config.inputHeight);
    config.keepFramesOnDevice = true;

    if (!m_hardwareFilterGraph.initialize(config, error)) {
        return false;
    }

    m_hardwareFilterGraphInitialized = true;
    return true;
}

bool FFmpegVideoTranscodePipeline::initializePacketWriter(std::string* error)
{
    FFmpegVideoPipeline::Config config;
    config.encoderCtx = encoderContext();
    config.outputFmtCtx = m_outputFmtCtx;
    config.outputVideoStream = m_outputVideoStream;

    return m_packetWriter.initialize(config, error);
}

bool FFmpegVideoTranscodePipeline::allocateFrames(std::string* error)
{
    m_decodedFrame = av_frame_alloc();
    m_filteredFrame = av_frame_alloc();
    m_softwareTransferFrame = av_frame_alloc();

    if (!m_decodedFrame || !m_filteredFrame || !m_softwareTransferFrame) {
        if (error) {
            *error = "av_frame_alloc video frame failed";
        }
        return false;
    }

    return true;
}

bool FFmpegVideoTranscodePipeline::processPacket(
    AVPacket* packet,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderStage.sendPacket(packet, error)) {
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_decoderStage.isInitialized()) {
        return true;
    }

    if (!m_decoderStage.sendFlush(error)) {
        return false;
    }

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushFilterAndEncoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (m_zeroCopyPipeline) {
        if (m_hardwareFilterGraphInitialized) {
            if (!m_hardwareFilterGraph.flush(error)) {
                return false;
            }

            if (!drainHardwareFilterGraph(error, onPacketWritten)) {
                return false;
            }
        }

        return writeEncodedPackets(nullptr, error, onPacketWritten);
    }

    if (!m_filterGraph.flush(error)) {
        return false;
    }

    if (!drainFilterGraph(error, onPacketWritten)) {
        return false;
    }

    return writeEncodedPackets(nullptr, error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::drainDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    while (true) {
        const int receiveRet = m_decoderStage.receiveFrame(m_decodedFrame, error);
        if (receiveRet == 0) {
            return true;
        }

        if (receiveRet < 0) {
            return false;
        }

        const bool ok = processDecodedFrame(error, onPacketWritten);
        av_frame_unref(m_decodedFrame);

        if (!ok) {
            return false;
        }
    }
}

bool FFmpegVideoTranscodePipeline::processDecodedFrame(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    const HardwareDecoderSupport::Config& hardwareDecoderConfig =
        m_decoderStage.hardwareDecoderConfig();
    const bool isExpectedHardwareFrame =
        m_decoderStage.usesHardwareFrames() &&
        hardwareDecoderConfig.valid &&
        m_decodedFrame->format == hardwareDecoderConfig.hardwarePixelFormat;

    if (m_zeroCopyPipeline && isExpectedHardwareFrame) {
        return processHardwareFrameZeroCopy(error, onPacketWritten);
    }

    if (m_zeroCopyPipeline && !isExpectedHardwareFrame) {
        if (error) {
            *error = "zero-copy pipeline expected hardware frame but decoder returned software frame";
        }
        return false;
    }

    AVFrame* frameForFilter = m_decodedFrame;

    if (isExpectedHardwareFrame) {
        if (!transferHardwareFrameToSoftware(m_decodedFrame, m_softwareTransferFrame, error)) {
            return false;
        }

        frameForFilter = m_softwareTransferFrame;
    }

    const bool ok = processFrameThroughSoftwareFilter(
        frameForFilter,
        error,
        onPacketWritten
    );

    if (frameForFilter == m_softwareTransferFrame) {
        av_frame_unref(m_softwareTransferFrame);
    }

    return ok;
}

bool FFmpegVideoTranscodePipeline::processHardwareFrameZeroCopy(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!normalizeFramePts(m_decodedFrame, error)) {
        return false;
    }

    if (!m_hardwareFilterGraphInitialized) {
        if (!initializeHardwareFilterGraphFromFrame(m_decodedFrame, error)) {
            return false;
        }
    }

    if (!m_hardwareFilterGraph.sendFrame(m_decodedFrame, error)) {
        return false;
    }

    return drainHardwareFilterGraph(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::processFrameThroughSoftwareFilter(
    AVFrame* frame,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!normalizeFramePts(frame, error)) {
        return false;
    }

    if (!m_filterGraph.sendFrame(frame, error)) {
        return false;
    }

    return drainFilterGraph(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::transferHardwareFrameToSoftware(AVFrame* hardwareFrame,
                                                                   AVFrame* softwareFrame,
                                                                   std::string* error) const
{
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
        AVCodecContext* decoderCtx = m_decoderStage.context();
        spdlog::warn(
            "[ZC][CPU_TRANSFER] hardware-to-software transfer enabled: hw_fmt={}, sw_fmt={}",
            pixelFormatName(static_cast<AVPixelFormat>(hardwareFrame->format)),
            pixelFormatName(decoderCtx ? decoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE)
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

bool FFmpegVideoTranscodePipeline::drainFilterGraph(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    AVCodecContext* encoderCtx = encoderContext();
    if (!encoderCtx) {
        if (error) {
            *error = "drain software filter graph failed: encoder stage is not initialized";
        }
        return false;
    }

    while (true) {
        const int receiveRet = m_filterGraph.receiveFrame(m_filteredFrame, error);

        if (receiveRet == 0) {
            return true;
        }

        if (receiveRet < 0) {
            return false;
        }

        const AVRational filterTimeBase = m_filterGraph.sinkTimeBase();

        if (m_filteredFrame->pts == AV_NOPTS_VALUE) {
            av_frame_unref(m_filteredFrame);
            if (error) {
                *error = "filtered video frame has invalid pts";
            }
            return false;
        }

        m_filteredFrame->pts = av_rescale_q(
            m_filteredFrame->pts,
            filterTimeBase,
            encoderCtx->time_base
        );

        if (m_lastSubmittedPts != AV_NOPTS_VALUE &&
            m_filteredFrame->pts <= m_lastSubmittedPts) {
            std::ostringstream oss;
            oss << "filtered video timestamp is not strictly increasing: current="
                << m_filteredFrame->pts
                << ", last="
                << m_lastSubmittedPts;

            av_frame_unref(m_filteredFrame);
            if (error) {
                *error = oss.str();
            }
            return false;
        }

        m_lastSubmittedPts = m_filteredFrame->pts;

        const bool ok = writeEncodedPackets(m_filteredFrame, error, onPacketWritten);
        av_frame_unref(m_filteredFrame);

        if (!ok) {
            return false;
        }
    }
}

bool FFmpegVideoTranscodePipeline::drainHardwareFilterGraph(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    AVCodecContext* encoderCtx = encoderContext();
    if (!encoderCtx) {
        if (error) {
            *error = "drain hardware filter graph failed: encoder stage is not initialized";
        }
        return false;
    }

    while (true) {
        const int receiveRet = m_hardwareFilterGraph.receiveFrame(m_filteredFrame, error);

        if (receiveRet == 0) {
            return true;
        }

        if (receiveRet < 0) {
            return false;
        }

        const AVRational filterTimeBase = m_hardwareFilterGraph.sinkTimeBase();

        if (m_filteredFrame->pts == AV_NOPTS_VALUE) {
            av_frame_unref(m_filteredFrame);
            if (error) {
                *error = "hardware filtered video frame has invalid pts";
            }
            return false;
        }

        m_filteredFrame->pts = av_rescale_q(
            m_filteredFrame->pts,
            filterTimeBase,
            encoderCtx->time_base
        );

        if (m_lastSubmittedPts != AV_NOPTS_VALUE &&
            m_filteredFrame->pts <= m_lastSubmittedPts) {
            av_frame_unref(m_filteredFrame);
            continue;
        }

        m_lastSubmittedPts = m_filteredFrame->pts;

        spdlog::debug(
            "[ZC][ENCODE] filtered_fmt={}, hw_frames_ctx={}, encoder_pix_fmt={}",
            pixelFormatName(static_cast<AVPixelFormat>(m_filteredFrame->format)),
            m_filteredFrame->hw_frames_ctx != nullptr,
            pixelFormatName(encoderCtx->pix_fmt)
        );

        const bool ok = writeEncodedPackets(m_filteredFrame, error, onPacketWritten);
        av_frame_unref(m_filteredFrame);

        if (!ok) {
            return false;
        }
    }
}

bool FFmpegVideoTranscodePipeline::writeEncodedPackets(
    AVFrame* frame,
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
{
    if (!m_packetWriter.sendFrame(frame, error)) {
        return false;
    }

    const int writtenPackets = m_packetWriter.receiveAndWritePackets(
        error,
        [&](int64_t packetCount, int64_t outTimeMs) {
            m_packetCount = packetCount;
            m_lastWrittenOutTimeMs = outTimeMs;

            if (onPacketWritten) {
                onPacketWritten(packetCount, outTimeMs);
            }
        }
    );

    return writtenPackets >= 0;
}

AVCodecContext* FFmpegVideoTranscodePipeline::encoderContext() const
{
    return m_encoderStage.context();
}

int64_t FFmpegVideoTranscodePipeline::decodedFrameTimestamp(const AVFrame* frame)
{
    if (!frame) {
        return AV_NOPTS_VALUE;
    }

    if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
        return frame->best_effort_timestamp;
    }

    if (frame->pts != AV_NOPTS_VALUE) {
        return frame->pts;
    }

    if (frame->pkt_dts != AV_NOPTS_VALUE) {
        return frame->pkt_dts;
    }

    return AV_NOPTS_VALUE;
}

bool FFmpegVideoTranscodePipeline::normalizeFramePts(AVFrame* frame, std::string* error) const
{
    if (!frame) {
        if (error) {
            *error = "normalize video frame pts failed: frame is null";
        }
        return false;
    }

    const int64_t inputVideoTs = decodedFrameTimestamp(frame);
    if (inputVideoTs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "input video frame has no valid timestamp; refuse to synthesize PTS in normalized transcoder";
        }
        return false;
    }

    const int64_t inputVideoUs = TimelineNormalizer::toUs(inputVideoTs, m_inputVideoStream->time_base);
    const int64_t normalizedVideoUs = m_timeline->normalizeUs(inputVideoUs);

    if (normalizedVideoUs == AV_NOPTS_VALUE) {
        if (error) {
            *error = "failed to normalize input video timestamp";
        }
        return false;
    }

    frame->pts = TimelineNormalizer::fromUs(normalizedVideoUs, m_inputVideoStream->time_base);
    if (frame->pts == AV_NOPTS_VALUE) {
        if (error) {
            *error = "decoded video frame pts is invalid after normalization";
        }
        return false;
    }

    return true;
}

bool FFmpegVideoTranscodePipeline::isInitialized() const
{
    return m_decoderStage.isInitialized() && m_encoderStage.isInitialized();
}

AVStream* FFmpegVideoTranscodePipeline::outputStream() const
{
    return m_encoderStage.outputStream();
}

int64_t FFmpegVideoTranscodePipeline::packetCount() const
{
    return m_packetCount;
}

int64_t FFmpegVideoTranscodePipeline::lastWrittenOutTimeMs() const
{
    return m_lastWrittenOutTimeMs;
}

int64_t FFmpegVideoTranscodePipeline::estimatedOutTimeMs() const
{
    if (m_lastWrittenOutTimeMs > 0) {
        return m_lastWrittenOutTimeMs;
    }

    AVCodecContext* encoderCtx = encoderContext();
    if (encoderCtx && encoderCtx->framerate.num > 0) {
        return static_cast<int64_t>(
            m_packetCount * 1000.0 *
            encoderCtx->framerate.den /
            encoderCtx->framerate.num
        );
    }

    return 0;
}

} // namespace media::ffmpeg
