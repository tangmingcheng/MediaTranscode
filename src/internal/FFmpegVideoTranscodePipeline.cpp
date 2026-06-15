#include "internal/FFmpegVideoTranscodePipeline.h"

#include "internal/FFmpegTimelineNormalizer.h"
#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <sstream>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mathematics.h>
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
    m_hardwareFramesContext.reset();
    m_hardwareDeviceContext.reset();

    if (m_softwareTransferFrame) {
        av_frame_free(&m_softwareTransferFrame);
    }

    if (m_filteredFrame) {
        av_frame_free(&m_filteredFrame);
    }

    if (m_decodedFrame) {
        av_frame_free(&m_decodedFrame);
    }

    if (m_decoderCtx) {
        avcodec_free_context(&m_decoderCtx);
    }

    if (m_encoderCtx) {
        avcodec_free_context(&m_encoderCtx);
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
    m_hardwareEncoderSelection = HardwareEncoderSelection{};
    m_hardwareDecoderConfig = HardwareDecoderSupport::Config{};
    m_hardwareDeviceAttachedToDecoder = false;
    m_hardwareDeviceAttachedToEncoder = false;
    m_decoderUsesHardwareFrames = false;
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

    if (config.hardwarePlan && config.hardwarePlan->valid && config.hardwarePlan->zeroCopy) {
        m_hardwarePlan = *config.hardwarePlan;
        m_hasHardwarePlan = true;
        m_hardwareBackend = m_hardwarePlan.backend;
        m_hardwareDecoderConfig = m_hardwarePlan.decoderConfig;
        m_hardwareEncoderSelection = m_hardwarePlan.encoderSelection;
    }

    return openDecoder(error) &&
        openEncoderAndCreateOutputStream(error) &&
        initializeFilterGraph(error) &&
        initializePacketWriter(error) &&
        allocateFrames(error);
}

bool FFmpegVideoTranscodePipeline::openDecoder(std::string* error)
{
    const AVCodec* decoder = avcodec_find_decoder(m_inputVideoStream->codecpar->codec_id);
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

    int ret = avcodec_parameters_to_context(m_decoderCtx, m_inputVideoStream->codecpar);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_to_context decoder failed: " + errorString(ret);
        }
        return false;
    }

    if (!initializeHardwareDeviceForDecoder(decoder, error)) {
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

bool FFmpegVideoTranscodePipeline::initializeHardwareDeviceForDecoder(const AVCodec* decoder,
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
    m_decoderCtx->get_format = &FFmpegVideoTranscodePipeline::selectDecoderPixelFormat;

    m_hardwareDeviceAttachedToDecoder = true;
    m_decoderUsesHardwareFrames = true;
    return true;
}

bool FFmpegVideoTranscodePipeline::openEncoderAndCreateOutputStream(std::string* error)
{
    const bool wantsHardwarePipeline =
        m_hasHardwarePlan &&
        m_decoderUsesHardwareFrames &&
        m_hardwareDeviceContext.isInitialized();

    const AVCodec* encoder = nullptr;

    if (wantsHardwarePipeline) {
        encoder = m_hardwareEncoderSelection.encoder;
        if (!encoder) {
            if (error) {
                *error = "zero-copy hardware pipeline unavailable: planner returned no encoder";
            }
            return false;
        }
    }

    if (!encoder) {
        const char* encoderName = preferredVideoEncoderName(m_config.videoCodec);
        if (encoderName) {
            encoder = avcodec_find_encoder_by_name(encoderName);
        }
    }

    if (!encoder) {
        const AVCodecID codecId = fallbackVideoCodecId(m_config.videoCodec);
        encoder = avcodec_find_encoder(codecId);
    }

    if (!encoder) {
        if (error) {
            *error = "avcodec_find_encoder failed: no suitable video encoder";
        }
        return false;
    }

    m_encoderCtx = avcodec_alloc_context3(encoder);
    if (!m_encoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 encoder failed";
        }
        return false;
    }

    m_outputFps = chooseOutputFps(m_config, m_inputVideoStream);
    m_enableConstantFps = m_config.fps > 0;

    int outputWidth = m_config.width > 0 ? m_config.width : m_decoderCtx->width;
    int outputHeight = m_config.height > 0 ? m_config.height : m_decoderCtx->height;

    outputWidth = normalizeEvenSize(outputWidth);
    outputHeight = normalizeEvenSize(outputHeight);

    if (outputWidth <= 0 || outputHeight <= 0) {
        if (error) {
            *error = "invalid output video size";
        }
        return false;
    }

    m_encoderCtx->width = outputWidth;
    m_encoderCtx->height = outputHeight;

    AVRational encoderTimeBase = AVRational{ 1, m_outputFps };
    if (!m_enableConstantFps) {
        encoderTimeBase = m_inputVideoStream->time_base;
        if (encoderTimeBase.num <= 0 || encoderTimeBase.den <= 0) {
            encoderTimeBase = AVRational{ 1, m_outputFps };
        }
    }

    m_encoderCtx->time_base = encoderTimeBase;
    m_encoderCtx->framerate = AVRational{ m_outputFps, 1 };
    m_encoderCtx->pix_fmt = m_hardwareEncoderSelection.encoder == encoder &&
        m_hardwareEncoderSelection.pixelFormat != AV_PIX_FMT_NONE
        ? m_hardwareEncoderSelection.pixelFormat
        : chooseVideoEncoderPixelFormat(encoder);
    m_encoderCtx->bit_rate = static_cast<int64_t>(std::max(1, m_config.videoBitrateKbps)) * 1000;
    m_encoderCtx->gop_size = std::max(10, m_outputFps * 2);
    m_encoderCtx->max_b_frames = 0;

    if (m_outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!initializeHardwareDeviceForEncoder(encoder, error)) {
        return false;
    }

    m_zeroCopyPipeline = m_hardwareEncoderSelection.encoder == encoder &&
        m_hardwareEncoderSelection.zeroCopy &&
        m_decoderUsesHardwareFrames &&
        m_hardwareDeviceAttachedToDecoder &&
        m_hardwareDeviceAttachedToEncoder;

    if (wantsHardwarePipeline && !m_zeroCopyPipeline) {
        if (error) {
            std::ostringstream oss;
            oss << "zero-copy hardware pipeline unavailable during execution: encoder="
                << (encoder->name ? encoder->name : "unknown")
                << ", backend="
                << (m_hardwareBackend.name ? m_hardwareBackend.name : "unknown")
                << ", selected_pix_fmt="
                << pixelFormatName(m_encoderCtx->pix_fmt);
            *error = oss.str();
        }
        return false;
    }

    setVideoEncoderOptions(m_encoderCtx, encoder);

    int ret = avcodec_open2(m_encoderCtx, encoder, nullptr);
    if (ret < 0) {
        if (error) {
            *error = std::string("avcodec_open2 encoder failed [") +
                (encoder->name ? encoder->name : "unknown") + "]: " + errorString(ret);
        }
        return false;
    }

    m_outputVideoStream = avformat_new_stream(m_outputFmtCtx, nullptr);
    if (!m_outputVideoStream) {
        if (error) {
            *error = "avformat_new_stream video failed";
        }
        return false;
    }

    m_outputVideoStream->time_base = m_encoderCtx->time_base;

    ret = avcodec_parameters_from_context(m_outputVideoStream->codecpar, m_encoderCtx);
    if (ret < 0) {
        if (error) {
            *error = "avcodec_parameters_from_context video failed: " + errorString(ret);
        }
        return false;
    }

    m_outputVideoStream->codecpar->codec_tag = 0;
    return true;
}

bool FFmpegVideoTranscodePipeline::initializeHardwareDeviceForEncoder(const AVCodec* encoder,
                                                                      std::string* error)
{
    if (!m_hasHardwarePlan) {
        return true;
    }

    const bool selectedHardwareEncoder =
        m_hardwareEncoderSelection.encoder == encoder &&
        m_hardwareEncoderSelection.hardwareEncoder;

    if (!selectedHardwareEncoder) {
        if (error) {
            *error = "zero-copy hardware pipeline unavailable: selected encoder is not a hardware encoder";
        }
        return false;
    }

    if (!m_hardwareDeviceContext.isInitialized()) {
        std::string hardwareError;
        if (!m_hardwareDeviceContext.initialize(
                m_hardwareEncoderSelection.backend.deviceType,
                nullptr,
                encoder,
                &hardwareError)) {
            if (error) {
                *error = hardwareError;
            }
            return false;
        }
    }

    AVBufferRef* deviceRef = m_hardwareDeviceContext.ref();
    if (!deviceRef) {
        if (error) {
            *error = "hardware device initialization failed: unable to reference device context";
        }
        return false;
    }

    if (m_encoderCtx->hw_device_ctx) {
        av_buffer_unref(&m_encoderCtx->hw_device_ctx);
    }

    m_encoderCtx->hw_device_ctx = deviceRef;
    m_hardwareDeviceAttachedToEncoder = true;
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
    VideoFilterGraph::Config config;
    config.decoderCtx = m_decoderCtx;
    config.encoderCtx = m_encoderCtx;
    config.inputStream = m_inputVideoStream;
    config.outputFps = m_outputFps;
    config.enableConstantFps = m_enableConstantFps;

    if (m_decoderUsesHardwareFrames) {
        config.inputPixelFormat = expectedSoftwareFormatAfterHardwareDownload(
            m_hardwareDeviceContext.resolvedDeviceType(),
            m_decoderCtx ? m_decoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE
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

    HardwareVideoFilterGraph::Config config;
    config.inputStream = m_inputVideoStream;
    config.inputHardwareFramesContext = frame->hw_frames_ctx;
    config.deviceType = m_hardwareBackend.deviceType;
    config.inputHardwarePixelFormat = static_cast<AVPixelFormat>(frame->format);
    config.softwarePixelFormat = m_encoderCtx ? m_encoderCtx->pix_fmt : AV_PIX_FMT_NONE;
    config.inputWidth = frame->width;
    config.inputHeight = frame->height;
    config.outputWidth = m_encoderCtx ? m_encoderCtx->width : frame->width;
    config.outputHeight = m_encoderCtx ? m_encoderCtx->height : frame->height;
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
    config.encoderCtx = m_encoderCtx;
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
    if (!m_decoderCtx) {
        if (error) {
            *error = "FFmpegVideoTranscodePipeline processPacket failed: decoder is not initialized";
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

    return drainDecoder(error, onPacketWritten);
}

bool FFmpegVideoTranscodePipeline::flushDecoder(
    std::string* error,
    const PacketWrittenCallback& onPacketWritten)
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
        const int ret = avcodec_receive_frame(m_decoderCtx, m_decodedFrame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        }

        if (ret < 0) {
            if (error) {
                *error = "avcodec_receive_frame decoder failed: " + errorString(ret);
            }
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
    const bool isExpectedHardwareFrame =
        m_decoderUsesHardwareFrames &&
        m_hardwareDecoderConfig.valid &&
        m_decodedFrame->format == m_hardwareDecoderConfig.hardwarePixelFormat;

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

    spdlog::warn(
        "[ZC][CPU_TRANSFER] av_hwframe_transfer_data called: hw_fmt={}, sw_fmt={}",
        pixelFormatName(static_cast<AVPixelFormat>(hardwareFrame->format)),
        pixelFormatName(m_decoderCtx ? m_decoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE)
    );

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
            m_encoderCtx->time_base
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
            m_encoderCtx->time_base
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
            pixelFormatName(m_encoderCtx ? m_encoderCtx->pix_fmt : AV_PIX_FMT_NONE)
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

int64_t FFmpegVideoTranscodePipeline::decodedFrameTimestamp() const
{
    if (!m_decodedFrame) {
        return AV_NOPTS_VALUE;
    }

    if (m_decodedFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
        return m_decodedFrame->best_effort_timestamp;
    }

    if (m_decodedFrame->pts != AV_NOPTS_VALUE) {
        return m_decodedFrame->pts;
    }

    if (m_decodedFrame->pkt_dts != AV_NOPTS_VALUE) {
        return m_decodedFrame->pkt_dts;
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

    const int64_t inputVideoTs = decodedFrameTimestamp();
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

AVPixelFormat FFmpegVideoTranscodePipeline::selectDecoderPixelFormat(
    AVCodecContext* ctx,
    const AVPixelFormat* formats)
{
    auto* self = ctx ? static_cast<FFmpegVideoTranscodePipeline*>(ctx->opaque) : nullptr;

    if (self && self->m_hardwareDecoderConfig.valid) {
        for (const AVPixelFormat* p = formats; p && *p != AV_PIX_FMT_NONE; ++p) {
            if (*p == self->m_hardwareDecoderConfig.hardwarePixelFormat) {
                return *p;
            }
        }
    }

    return formats ? formats[0] : AV_PIX_FMT_NONE;
}

bool FFmpegVideoTranscodePipeline::isInitialized() const
{
    return m_decoderCtx && m_encoderCtx && m_outputVideoStream;
}

AVStream* FFmpegVideoTranscodePipeline::outputStream() const
{
    return m_outputVideoStream;
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

    if (m_encoderCtx && m_encoderCtx->framerate.num > 0) {
        return static_cast<int64_t>(
            m_packetCount * 1000.0 *
            m_encoderCtx->framerate.den /
            m_encoderCtx->framerate.num
        );
    }

    return 0;
}

} // namespace media::ffmpeg
