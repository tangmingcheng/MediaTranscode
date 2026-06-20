#include "internal/FFmpegVideoFilterStage.h"

#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <sstream>
#include <utility>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/mathematics.h>
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

bool shouldLogZeroCopyFrame(int64_t frameCount)
{
    return frameCount <= 3 || frameCount % 120 == 0;
}

AVRational sanitizeSampleAspectRatio(AVRational ratio)
{
    if (ratio.num > 0 && ratio.den > 0) {
        return ratio;
    }

    return AVRational{ 1, 1 };
}

AVRational chooseInputSampleAspectRatio(const AVFrame* frame,
                                        AVRational fallbackSampleAspectRatio)
{
    if (frame &&
        frame->sample_aspect_ratio.num > 0 &&
        frame->sample_aspect_ratio.den > 0) {
        return frame->sample_aspect_ratio;
    }

    return sanitizeSampleAspectRatio(fallbackSampleAspectRatio);
}

bool isHardwarePixelFormat(AVPixelFormat format)
{
    switch (format) {
    case AV_PIX_FMT_D3D11:
    case AV_PIX_FMT_CUDA:
    case AV_PIX_FMT_QSV:
    case AV_PIX_FMT_VAAPI:
    case AV_PIX_FMT_DRM_PRIME:
    case AV_PIX_FMT_VIDEOTOOLBOX:
        return true;
    default:
        return false;
    }
}

AVPixelFormat hardwareFrameSoftwareFormat(const AVFrame* frame)
{
    if (!frame || !frame->hw_frames_ctx || !frame->hw_frames_ctx->data) {
        return AV_PIX_FMT_NONE;
    }

    const auto* framesContext = reinterpret_cast<const AVHWFramesContext*>(frame->hw_frames_ctx->data);
    return framesContext ? framesContext->sw_format : AV_PIX_FMT_NONE;
}

AVPixelFormat encoderHardwareSoftwareFormat(const AVCodecContext* encoderCtx,
                                            const AVFrame* frame)
{
    if (encoderCtx && encoderCtx->sw_pix_fmt != AV_PIX_FMT_NONE) {
        return encoderCtx->sw_pix_fmt;
    }

    if (encoderCtx && !isHardwarePixelFormat(encoderCtx->pix_fmt)) {
        return encoderCtx->pix_fmt;
    }

    return hardwareFrameSoftwareFormat(frame);
}

} // namespace

FFmpegVideoFilterStage::~FFmpegVideoFilterStage()
{
    reset();
}

void FFmpegVideoFilterStage::reset()
{
    clearBypassedHardwareFrames();
    m_hardwareFilterGraph.reset();
    m_filterGraph.reset();

    m_encoderCtx = nullptr;
    m_inputMetadata = FFmpegVideoInputMetadata{};

    m_outputFps = 0;
    m_enableConstantFps = false;

    m_initialized = false;
    m_zeroCopyPipeline = false;
    m_bypassHardwareFilterGraph = false;
    m_softwareFilterGraphInitialized = false;
    m_hardwareFilterGraphInitialized = false;

    m_hardwareBackend = HardwareBackendProfile{};
    m_lastSubmittedPts = AV_NOPTS_VALUE;
    m_bypassedHardwareFrameLogCount = 0;
    m_filteredHardwareFrameLogCount = 0;
}

bool FFmpegVideoFilterStage::initialize(const Config& config, std::string* error)
{
    reset();

    if (!config.encoderCtx) {
        if (error) {
            *error = "FFmpegVideoFilterStage initialize failed: encoderCtx is null";
        }
        return false;
    }

    if (!config.inputMetadata.hasValidSize()) {
        if (error) {
            *error = "FFmpegVideoFilterStage initialize failed: input metadata has invalid video size";
        }
        return false;
    }

    if (!isValidRatio(config.inputMetadata.timeBase)) {
        if (error) {
            *error = "FFmpegVideoFilterStage initialize failed: input metadata time base is invalid";
        }
        return false;
    }

    m_encoderCtx = config.encoderCtx;
    m_inputMetadata = config.inputMetadata;
    m_inputMetadata.sampleAspectRatio = sanitizeSampleAspectRatio(
        m_inputMetadata.sampleAspectRatio
    );
    m_outputFps = config.outputFps;
    m_enableConstantFps = config.enableConstantFps;
    m_zeroCopyPipeline = config.zeroCopyPipeline;
    m_hardwareBackend = config.hardwareBackend;
    m_bypassHardwareFilterGraph = m_zeroCopyPipeline &&
        m_hardwareBackend.supportsDirectHardwareFrameEncode &&
        !m_hardwareBackend.supportsZeroCopyFilter;

    m_initialized = true;

    if (m_zeroCopyPipeline && m_bypassHardwareFilterGraph) {
        spdlog::info(
            "[ZC][FILTER] bypass hardware filter graph for backend={}",
            m_hardwareBackend.name ? m_hardwareBackend.name : "unknown"
        );
    }

    return true;
}

bool FFmpegVideoFilterStage::initializeSoftwareFilterGraph(
    const AVFrame* firstFrame,
    std::string* error)
{
    if (!firstFrame) {
        if (error) {
            *error = "initialize software filter graph failed: first frame is null";
        }
        return false;
    }

    const AVPixelFormat inputFormat = static_cast<AVPixelFormat>(firstFrame->format);
    if (inputFormat == AV_PIX_FMT_NONE || firstFrame->width <= 0 || firstFrame->height <= 0) {
        if (error) {
            std::ostringstream oss;
            oss << "initialize software filter graph failed: invalid first frame metadata, fmt="
                << pixelFormatName(inputFormat)
                << ", size="
                << firstFrame->width
                << "x"
                << firstFrame->height;
            *error = oss.str();
        }
        return false;
    }

    VideoFilterGraph::Config config;
    config.encoderCtx = m_encoderCtx;
    config.inputPixelFormat = inputFormat;
    config.inputWidth = firstFrame->width;
    config.inputHeight = firstFrame->height;
    config.inputSampleAspectRatio = chooseInputSampleAspectRatio(
        firstFrame,
        m_inputMetadata.sampleAspectRatio
    );
    config.inputTimeBase = m_inputMetadata.timeBase;
    config.inputFrameRate = m_inputMetadata.frameRate;
    config.outputFps = m_outputFps;
    config.enableConstantFps = m_enableConstantFps;

    if (!m_filterGraph.initialize(config, error)) {
        return false;
    }

    m_softwareFilterGraphInitialized = true;

    spdlog::info(
        "[FILTER] software graph initialized from first frame: input_fmt={}, input_size={}x{}, sar={}/{}, encoder_pix_fmt={}, encoder_size={}x{}",
        pixelFormatName(config.inputPixelFormat),
        config.inputWidth,
        config.inputHeight,
        config.inputSampleAspectRatio.num,
        config.inputSampleAspectRatio.den,
        pixelFormatName(m_encoderCtx ? m_encoderCtx->pix_fmt : AV_PIX_FMT_NONE),
        m_encoderCtx ? m_encoderCtx->width : 0,
        m_encoderCtx ? m_encoderCtx->height : 0
    );

    return true;
}

bool FFmpegVideoFilterStage::initializeHardwareFilterGraphFromFrame(
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

    const AVPixelFormat inputSoftwareFormat = hardwareFrameSoftwareFormat(frame);
    const AVPixelFormat outputSoftwareFormat = encoderHardwareSoftwareFormat(m_encoderCtx, frame);
    const VideoColorMetadata colorMetadata = VideoColorMetadata::fromFrame(
        frame,
        m_inputMetadata.sampleAspectRatio
    );

    HardwareVideoFilterGraph::Config config;
    config.inputHardwareFramesContext = frame->hw_frames_ctx;
    config.backend = m_hardwareBackend;
    config.deviceType = m_hardwareBackend.deviceType;
    config.inputHardwarePixelFormat = static_cast<AVPixelFormat>(frame->format);
    config.inputSoftwarePixelFormat = inputSoftwareFormat;
    config.softwarePixelFormat = outputSoftwareFormat;
    config.inputWidth = frame->width;
    config.inputHeight = frame->height;
    config.outputWidth = m_encoderCtx ? m_encoderCtx->width : frame->width;
    config.outputHeight = m_encoderCtx ? m_encoderCtx->height : frame->height;
    config.inputTimeBase = m_inputMetadata.timeBase;
    config.inputFrameRate = m_inputMetadata.frameRate;
    config.colorMetadata = colorMetadata;
    config.outputFps = m_outputFps;
    config.enableConstantFps = m_enableConstantFps;
    config.enableScale = config.outputWidth > 0 &&
        config.outputHeight > 0 &&
        (config.outputWidth != config.inputWidth || config.outputHeight != config.inputHeight);
    config.enableFormatConversion = inputSoftwareFormat != AV_PIX_FMT_NONE &&
        outputSoftwareFormat != AV_PIX_FMT_NONE &&
        inputSoftwareFormat != outputSoftwareFormat;
    config.keepFramesOnDevice = true;

    if (!m_hardwareFilterGraph.initialize(config, error)) {
        return false;
    }

    m_hardwareFilterGraphInitialized = true;

    spdlog::info(
        "[ZC][FILTER] hardware graph initialized: backend={}, hw_fmt={}, input_sw_fmt={}, output_sw_fmt={}, scale={}, format_convert={}, input_size={}x{}, output_size={}x{}, color_metadata={}",
        m_hardwareBackend.name ? m_hardwareBackend.name : "unknown",
        pixelFormatName(config.inputHardwarePixelFormat),
        pixelFormatName(config.inputSoftwarePixelFormat),
        pixelFormatName(config.softwarePixelFormat),
        config.enableScale,
        config.enableFormatConversion,
        config.inputWidth,
        config.inputHeight,
        config.outputWidth,
        config.outputHeight,
        VideoColorMetadataUtils::describe(colorMetadata)
    );

    return true;
}

bool FFmpegVideoFilterStage::sendSoftwareFrame(AVFrame* frame, std::string* error)
{
    if (!m_initialized) {
        if (error) {
            *error = "FFmpegVideoFilterStage sendSoftwareFrame failed: stage is not initialized";
        }
        return false;
    }

    if (m_zeroCopyPipeline) {
        if (error) {
            *error = "software filter path is unavailable in zero-copy pipeline";
        }
        return false;
    }

    if (!frame) {
        if (error) {
            *error = "software filter frame is null";
        }
        return false;
    }

    if (!m_softwareFilterGraphInitialized) {
        if (!initializeSoftwareFilterGraph(frame, error)) {
            return false;
        }
    }

    return m_filterGraph.sendFrame(frame, error);
}

bool FFmpegVideoFilterStage::sendHardwareFrame(AVFrame* frame, std::string* error)
{
    if (!m_initialized) {
        if (error) {
            *error = "FFmpegVideoFilterStage sendHardwareFrame failed: stage is not initialized";
        }
        return false;
    }

    if (!m_zeroCopyPipeline) {
        if (error) {
            *error = "hardware filter path is unavailable in software pipeline";
        }
        return false;
    }

    if (m_bypassHardwareFilterGraph) {
        return queueBypassedHardwareFrame(frame, error);
    }

    if (!m_hardwareFilterGraphInitialized) {
        if (!initializeHardwareFilterGraphFromFrame(frame, error)) {
            return false;
        }
    }

    return m_hardwareFilterGraph.sendFrame(frame, error);
}

bool FFmpegVideoFilterStage::flush(std::string* error)
{
    if (!m_initialized) {
        return true;
    }

    if (m_zeroCopyPipeline) {
        if (m_bypassHardwareFilterGraph) {
            return true;
        }

        if (!m_hardwareFilterGraphInitialized) {
            return true;
        }

        return m_hardwareFilterGraph.flush(error);
    }

    if (!m_softwareFilterGraphInitialized) {
        return true;
    }

    return m_filterGraph.flush(error);
}

int FFmpegVideoFilterStage::receiveFrame(AVFrame* frame, std::string* error)
{
    if (!m_initialized) {
        if (error) {
            *error = "FFmpegVideoFilterStage receiveFrame failed: stage is not initialized";
        }
        return -1;
    }

    if (m_zeroCopyPipeline) {
        if (m_bypassHardwareFilterGraph) {
            return receiveBypassedHardwareFrame(frame, error);
        }
        return receiveHardwareFrame(frame, error);
    }

    return receiveSoftwareFrame(frame, error);
}

bool FFmpegVideoFilterStage::queueBypassedHardwareFrame(AVFrame* frame, std::string* error)
{
    if (!frame) {
        if (error) {
            *error = "bypass hardware filter failed: frame is null";
        }
        return false;
    }

    FramePtr queued = makeFrame();
    if (!queued) {
        if (error) {
            *error = "av_frame_alloc bypass hardware frame failed";
        }
        return false;
    }

    const int ret = av_frame_ref(queued.get(), frame);
    if (ret < 0) {
        if (error) {
            *error = "av_frame_ref bypass hardware frame failed: " + errorString(ret);
        }
        return false;
    }

    m_bypassedHardwareFrames.push_back(std::move(queued));
    return true;
}

int FFmpegVideoFilterStage::receiveBypassedHardwareFrame(AVFrame* frame, std::string* error)
{
    if (!frame) {
        if (error) {
            *error = "receive bypassed hardware frame failed: frame is null";
        }
        return -1;
    }

    if (!isValidRatio(m_inputMetadata.timeBase)) {
        if (error) {
            *error = "receive bypassed hardware frame failed: input time base is invalid";
        }
        return -1;
    }

    while (!m_bypassedHardwareFrames.empty()) {
        FramePtr queued = std::move(m_bypassedHardwareFrames.front());
        m_bypassedHardwareFrames.pop_front();

        av_frame_unref(frame);
        av_frame_move_ref(frame, queued.get());

        const bool ok = rescaleAndValidateFramePts(
            frame,
            m_inputMetadata.timeBase,
            true,
            error
        );

        if (ok) {
            ++m_bypassedHardwareFrameLogCount;
            if (shouldLogZeroCopyFrame(m_bypassedHardwareFrameLogCount)) {
                spdlog::debug(
                    "[ZC][ENCODE] bypassed_frame={} fmt={}, hw_frames_ctx={}, encoder_pix_fmt={}",
                    m_bypassedHardwareFrameLogCount,
                    pixelFormatName(static_cast<AVPixelFormat>(frame->format)),
                    frame->hw_frames_ctx != nullptr,
                    pixelFormatName(m_encoderCtx ? m_encoderCtx->pix_fmt : AV_PIX_FMT_NONE)
                );
            }
            return 1;
        }

        if (!error || error->empty()) {
            av_frame_unref(frame);
            continue;
        }

        return -1;
    }

    return 0;
}

void FFmpegVideoFilterStage::clearBypassedHardwareFrames()
{
    m_bypassedHardwareFrames.clear();
}

int FFmpegVideoFilterStage::receiveSoftwareFrame(AVFrame* frame, std::string* error)
{
    if (!m_softwareFilterGraphInitialized) {
        return 0;
    }

    while (true) {
        const int receiveRet = m_filterGraph.receiveFrame(frame, error);
        if (receiveRet == 0) {
            return 0;
        }

        if (receiveRet < 0) {
            return -1;
        }

        const bool ok = rescaleAndValidateFramePts(
            frame,
            m_filterGraph.sinkTimeBase(),
            false,
            error
        );

        if (ok) {
            return 1;
        }

        return -1;
    }
}

int FFmpegVideoFilterStage::receiveHardwareFrame(AVFrame* frame, std::string* error)
{
    if (!m_hardwareFilterGraphInitialized) {
        return 0;
    }

    while (true) {
        const int receiveRet = m_hardwareFilterGraph.receiveFrame(frame, error);
        if (receiveRet == 0) {
            return 0;
        }

        if (receiveRet < 0) {
            return -1;
        }

        const bool ok = rescaleAndValidateFramePts(
            frame,
            m_hardwareFilterGraph.sinkTimeBase(),
            true,
            error
        );

        if (ok) {
            ++m_filteredHardwareFrameLogCount;
            if (shouldLogZeroCopyFrame(m_filteredHardwareFrameLogCount)) {
                spdlog::debug(
                    "[ZC][ENCODE] filtered_frame={} fmt={}, hw_frames_ctx={}, encoder_pix_fmt={}, encoder_sw_pix_fmt={}",
                    m_filteredHardwareFrameLogCount,
                    pixelFormatName(static_cast<AVPixelFormat>(frame->format)),
                    frame->hw_frames_ctx != nullptr,
                    pixelFormatName(m_encoderCtx ? m_encoderCtx->pix_fmt : AV_PIX_FMT_NONE),
                    pixelFormatName(m_encoderCtx ? m_encoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE)
                );
            }
            return 1;
        }

        if (!error || error->empty()) {
            av_frame_unref(frame);
            continue;
        }

        return -1;
    }
}

bool FFmpegVideoFilterStage::rescaleAndValidateFramePts(AVFrame* frame,
                                                        AVRational filterTimeBase,
                                                        bool dropNonIncreasingFrame,
                                                        std::string* error)
{
    if (!frame) {
        if (error) {
            *error = "filtered video frame is null";
        }
        return false;
    }

    if (!m_encoderCtx) {
        if (error) {
            *error = "filter stage failed: encoderCtx is null";
        }
        return false;
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        av_frame_unref(frame);
        if (error) {
            *error = m_zeroCopyPipeline
                ? "hardware filtered video frame has invalid pts"
                : "filtered video frame has invalid pts";
        }
        return false;
    }

    frame->pts = av_rescale_q(
        frame->pts,
        filterTimeBase,
        m_encoderCtx->time_base
    );

    if (m_lastSubmittedPts != AV_NOPTS_VALUE &&
        frame->pts <= m_lastSubmittedPts) {
        if (dropNonIncreasingFrame) {
            if (error) {
                error->clear();
            }
            return false;
        }

        std::ostringstream oss;
        oss << "filtered video timestamp is not strictly increasing: current="
            << frame->pts
            << ", last="
            << m_lastSubmittedPts;

        av_frame_unref(frame);
        if (error) {
            *error = oss.str();
        }
        return false;
    }

    m_lastSubmittedPts = frame->pts;
    return true;
}

bool FFmpegVideoFilterStage::isInitialized() const
{
    return m_initialized;
}

bool FFmpegVideoFilterStage::zeroCopyPipeline() const
{
    return m_zeroCopyPipeline;
}

bool FFmpegVideoFilterStage::hardwareFilterGraphInitialized() const
{
    return m_hardwareFilterGraphInitialized;
}

} // namespace media::ffmpeg
