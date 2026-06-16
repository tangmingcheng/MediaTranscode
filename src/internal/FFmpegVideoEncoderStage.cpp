#include "internal/FFmpegVideoEncoderStage.h"

#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <sstream>

extern "C" {
#include <libavutil/buffer.h>
#include <libavutil/pixdesc.h>
}

namespace media::ffmpeg {
namespace {

const char* pixelFormatName(AVPixelFormat format)
{
    const char* name = av_get_pix_fmt_name(format);
    return name ? name : "none";
}

bool isValidRatio(AVRational ratio)
{
    return ratio.num > 0 && ratio.den > 0;
}

int chooseOutputFpsFromMetadata(const TranscodeConfig& config,
                                const FFmpegVideoInputMetadata& inputMetadata)
{
    if (config.fps > 0) {
        return config.fps;
    }

    if (isValidRatio(inputMetadata.frameRate)) {
        const double fps = av_q2d(inputMetadata.frameRate);
        if (fps > 1.0 && fps < 240.0) {
            return static_cast<int>(std::round(fps));
        }
    }

    return 25;
}

AVRational chooseEncoderTimeBase(const FFmpegVideoInputMetadata& inputMetadata,
                                 int outputFps,
                                 bool enableConstantFps)
{
    if (enableConstantFps) {
        return AVRational{ 1, outputFps };
    }

    if (isValidRatio(inputMetadata.timeBase)) {
        return inputMetadata.timeBase;
    }

    return AVRational{ 1, outputFps };
}

} // namespace

FFmpegVideoEncoderStage::~FFmpegVideoEncoderStage()
{
    reset();
}

void FFmpegVideoEncoderStage::reset()
{
    if (m_encoderCtx) {
        avcodec_free_context(&m_encoderCtx);
    }

    m_config = TranscodeConfig{};
    m_inputMetadata = FFmpegVideoInputMetadata{};
    m_outputFmtCtx = nullptr;
    m_outputVideoStream = nullptr;

    m_hardwareDeviceContext = nullptr;
    m_hardwarePlan = HardwarePipelinePlan{};
    m_hardwareBackend = HardwareBackendProfile{};
    m_hardwareEncoderSelection = HardwareEncoderSelection{};

    m_hasHardwarePlan = false;
    m_decoderUsesHardwareFrames = false;
    m_decoderHardwareDeviceAttached = false;
    m_hardwareDeviceAttachedToEncoder = false;
    m_zeroCopyPipeline = false;

    m_outputFps = 0;
    m_enableConstantFps = false;
}

bool FFmpegVideoEncoderStage::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.transcodeConfig) {
        if (error) {
            *error = "FFmpegVideoEncoderStage initialize failed: transcodeConfig is null";
        }
        return false;
    }

    if (!config.inputMetadata.hasValidSize()) {
        if (error) {
            *error = "FFmpegVideoEncoderStage initialize failed: input metadata has invalid video size";
        }
        return false;
    }

    if (!config.outputFmtCtx) {
        if (error) {
            *error = "FFmpegVideoEncoderStage initialize failed: outputFmtCtx is null";
        }
        return false;
    }

    if (!config.outputFmtCtx->oformat) {
        if (error) {
            *error = "FFmpegVideoEncoderStage initialize failed: output format is null";
        }
        return false;
    }

    m_config = *config.transcodeConfig;
    m_inputMetadata = config.inputMetadata;
    m_outputFmtCtx = config.outputFmtCtx;
    m_hardwareDeviceContext = config.hardwareDeviceContext;
    m_decoderUsesHardwareFrames = config.decoderUsesHardwareFrames;
    m_decoderHardwareDeviceAttached = config.decoderHardwareDeviceAttached;

    if (config.hardwarePlan &&
        config.hardwarePlan->valid &&
        config.hardwarePlan->executionMode != VideoExecutionMode::Cpu) {
        m_hardwarePlan = *config.hardwarePlan;
        m_hardwareBackend = m_hardwarePlan.backend;
        m_hardwareEncoderSelection = m_hardwarePlan.encoderSelection;
        m_hasHardwarePlan = true;
    }

    return openEncoderAndCreateOutputStream(error);
}

bool FFmpegVideoEncoderStage::openEncoderAndCreateOutputStream(std::string* error)
{
    const bool wantsHardwarePipeline =
        m_hasHardwarePlan &&
        m_decoderUsesHardwareFrames;

    const AVCodec* encoder = nullptr;

    if (wantsHardwarePipeline) {
        encoder = m_hardwareEncoderSelection.encoder;
        if (!encoder) {
            if (error) {
                *error = "hardware video pipeline unavailable: planner returned no encoder";
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

    const bool selectedPlannedHardwareEncoder = m_hardwareEncoderSelection.encoder == encoder;
    const bool directHardwareFrameEncoder =
        selectedPlannedHardwareEncoder &&
        m_hardwareBackend.supportsDirectHardwareFrameEncode;

    m_encoderCtx = avcodec_alloc_context3(encoder);
    if (!m_encoderCtx) {
        if (error) {
            *error = "avcodec_alloc_context3 encoder failed";
        }
        return false;
    }

    m_outputFps = chooseOutputFpsFromMetadata(m_config, m_inputMetadata);
    m_enableConstantFps = m_config.fps > 0;

    if (m_outputFps <= 0) {
        if (error) {
            *error = "invalid output video fps";
        }
        return false;
    }

    int outputWidth = m_config.width > 0 ? m_config.width : m_inputMetadata.width;
    int outputHeight = m_config.height > 0 ? m_config.height : m_inputMetadata.height;

    outputWidth = normalizeEvenSize(outputWidth);
    outputHeight = normalizeEvenSize(outputHeight);

    if (outputWidth <= 0 || outputHeight <= 0) {
        if (error) {
            std::ostringstream oss;
            oss << "invalid output video size: requested="
                << m_config.width
                << "x"
                << m_config.height
                << ", input="
                << m_inputMetadata.width
                << "x"
                << m_inputMetadata.height;
            *error = oss.str();
        }
        return false;
    }

    m_encoderCtx->width = outputWidth;
    m_encoderCtx->height = outputHeight;

    m_encoderCtx->time_base = chooseEncoderTimeBase(
        m_inputMetadata,
        m_outputFps,
        m_enableConstantFps
    );
    m_encoderCtx->framerate = AVRational{ m_outputFps, 1 };
    m_encoderCtx->pix_fmt = selectedPlannedHardwareEncoder &&
        m_hardwareEncoderSelection.pixelFormat != AV_PIX_FMT_NONE
        ? m_hardwareEncoderSelection.pixelFormat
        : chooseVideoEncoderPixelFormat(encoder);

    if (directHardwareFrameEncoder &&
        m_hardwareBackend.directHardwareFrameSoftwareFormat != AV_PIX_FMT_NONE &&
        m_encoderCtx->sw_pix_fmt == AV_PIX_FMT_NONE) {
        m_encoderCtx->sw_pix_fmt = m_hardwareBackend.directHardwareFrameSoftwareFormat;
    }

    m_encoderCtx->bit_rate = static_cast<int64_t>(std::max(1, m_config.videoBitrateKbps)) * 1000;
    m_encoderCtx->gop_size = std::max(10, m_outputFps * 2);
    m_encoderCtx->max_b_frames = 0;

    if (m_outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        m_encoderCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!initializeHardwareDeviceForEncoder(encoder, error)) {
        return false;
    }

    const bool encoderHardwareReady = directHardwareFrameEncoder ||
        m_hardwareDeviceAttachedToEncoder;

    const bool decoderHardwareReady = directHardwareFrameEncoder ||
        m_decoderHardwareDeviceAttached;

    m_zeroCopyPipeline = selectedPlannedHardwareEncoder &&
        m_hardwareEncoderSelection.zeroCopy &&
        m_decoderUsesHardwareFrames &&
        decoderHardwareReady &&
        encoderHardwareReady;

    if (wantsHardwarePipeline &&
        m_hardwarePlan.executionMode == VideoExecutionMode::ZeroCopy &&
        !m_zeroCopyPipeline) {
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

    if (wantsHardwarePipeline &&
        m_hardwarePlan.executionMode == VideoExecutionMode::MixedGpu &&
        m_zeroCopyPipeline) {
        if (error) {
            *error = "mixed GPU fallback unexpectedly entered zero-copy execution";
        }
        return false;
    }

    if (wantsHardwarePipeline &&
        m_hardwarePlan.executionMode == VideoExecutionMode::MixedGpu) {
        spdlog::warn(
            "[PLAN] active mixed GPU path: hardware decode={}, hardware encode={}, encoder={}, encoder_pix_fmt={}",
            m_decoderHardwareDeviceAttached,
            m_hardwareDeviceAttachedToEncoder,
            encoder->name ? encoder->name : "unknown",
            pixelFormatName(m_encoderCtx->pix_fmt)
        );
    }

    setVideoEncoderOptions(m_encoderCtx, encoder);

    spdlog::info(
        "[ZC][ENCODER] open encoder={}, backend={}, pix_fmt={}, sw_pix_fmt={}, size={}x{}, fps={}/{}",
        encoder->name ? encoder->name : "unknown",
        m_hardwareBackend.name ? m_hardwareBackend.name : "none",
        pixelFormatName(m_encoderCtx->pix_fmt),
        pixelFormatName(m_encoderCtx->sw_pix_fmt),
        m_encoderCtx->width,
        m_encoderCtx->height,
        m_encoderCtx->framerate.num,
        m_encoderCtx->framerate.den
    );

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

bool FFmpegVideoEncoderStage::initializeHardwareDeviceForEncoder(const AVCodec* encoder,
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
            *error = "hardware video pipeline unavailable: selected encoder is not a hardware encoder";
        }
        return false;
    }

    if (m_hardwareBackend.supportsDirectHardwareFrameEncode &&
        !m_hardwareBackend.supportsZeroCopyFilter) {
        return true;
    }

    if (!m_hardwareDeviceContext || !m_hardwareDeviceContext->isInitialized()) {
        if (error) {
            *error = "hardware device initialization failed: decoder stage has no hardware device";
        }
        return false;
    }

    AVBufferRef* deviceRef = m_hardwareDeviceContext->ref();
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

bool FFmpegVideoEncoderStage::isInitialized() const
{
    return m_encoderCtx && m_outputVideoStream;
}

AVCodecContext* FFmpegVideoEncoderStage::context() const
{
    return m_encoderCtx;
}

AVStream* FFmpegVideoEncoderStage::outputStream() const
{
    return m_outputVideoStream;
}

int FFmpegVideoEncoderStage::outputFps() const
{
    return m_outputFps;
}

bool FFmpegVideoEncoderStage::enableConstantFps() const
{
    return m_enableConstantFps;
}

bool FFmpegVideoEncoderStage::hasHardwarePlan() const
{
    return m_hasHardwarePlan;
}

bool FFmpegVideoEncoderStage::hardwareDeviceAttached() const
{
    return m_hardwareDeviceAttachedToEncoder;
}

bool FFmpegVideoEncoderStage::zeroCopyPipeline() const
{
    return m_zeroCopyPipeline;
}

const HardwareBackendProfile& FFmpegVideoEncoderStage::hardwareBackend() const
{
    return m_hardwareBackend;
}

const HardwareEncoderSelection& FFmpegVideoEncoderStage::hardwareEncoderSelection() const
{
    return m_hardwareEncoderSelection;
}

} // namespace media::ffmpeg
