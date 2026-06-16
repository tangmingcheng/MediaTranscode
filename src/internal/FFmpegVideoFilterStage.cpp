#include "internal/FFmpegVideoFilterStage.h"

#include "internal/FFmpegUtils.h"

#include "spdlog/spdlog.h"

#include <sstream>

extern "C" {
#include <libavutil/avutil.h>
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
    case HardwareDeviceType::RKMPP:
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

bool shouldLogZeroCopyFrame(int64_t frameCount)
{
    return frameCount <= 3 || frameCount % 120 == 0;
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

    m_decoderCtx = nullptr;
    m_encoderCtx = nullptr;
    m_inputVideoStream = nullptr;

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

    if (!config.decoderCtx) {
        if (error) {
            *error = "FFmpegVideoFilterStage initialize failed: decoderCtx is null";
        }
        return false;
    }

    if (!config.encoderCtx) {
        if (error) {
            *error = "FFmpegVideoFilterStage initialize failed: encoderCtx is null";
        }
        return false;
    }

    if (!config.inputVideoStream) {
        if (error) {
            *error = "FFmpegVideoFilterStage initialize failed: inputVideoStream is null";
        }
        return false;
    }

    m_decoderCtx = config.decoderCtx;
    m_encoderCtx = config.encoderCtx;
    m_inputVideoStream = config.inputVideoStream;
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
    VideoFilterGraph::Config config;
    config.decoderCtx = m_decoderCtx;
    config.encoderCtx = m_encoderCtx;
    config.inputStream = m_inputVideoStream;
    config.outputFps = m_outputFps;
    config.enableConstantFps = m_enableConstantFps;

    if (firstFrame) {
        config.inputPixelFormat = static_cast<AVPixelFormat>(firstFrame->format);
        config.inputWidth = firstFrame->width;
        config.inputHeight = firstFrame->height;
    }
    else if (m_hardwareBackend.deviceType != HardwareDeviceType::None &&
        m_hardwareBackend.deviceType != HardwareDeviceType::Auto &&
        m_hardwareBackend.hardwarePixelFormat != AV_PIX_FMT_NONE) {
        config.inputPixelFormat = expectedSoftwareFormatAfterHardwareDownload(
            m_hardwareBackend.deviceType,
            m_decoderCtx ? m_decoderCtx->sw_pix_fmt : AV_PIX_FMT_NONE
        );
    }

    if (!m_filterGraph.initialize(config, error)) {
        return false;
    }

    m_softwareFilterGraphInitialized = true;

    spdlog::info(
        "[FILTER] software graph initialized: input_fmt={}, input_size={}x{}, encoder_pix_fmt={}, encoder_size={}x{}",
        pixelFormatName(config.inputPixelFormat != AV_PIX_FMT_NONE
            ? config.inputPixelFormat
            : (m_decoderCtx ? m_decoderCtx->pix_fmt : AV_PIX_FMT_NONE)),
        config.inputWidth > 0 ? config.inputWidth : (m_decoderCtx ? m_decoderCtx->width : 0),
        config.inputHeight > 0 ? config.inputHeight : (m_decoderCtx ? m_decoderCtx->height : 0),
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

    AVFrame* queued = av_frame_alloc();
    if (!queued) {
        if (error) {
            *error = "av_frame_alloc bypass hardware frame failed";
        }
        return false;
    }

    const int ret = av_frame_ref(queued, frame);
    if (ret < 0) {
        av_frame_free(&queued);
        if (error) {
            *error = "av_frame_ref bypass hardware frame failed: " + errorString(ret);
        }
        return false;
    }

    m_bypassedHardwareFrames.push_back(queued);
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

    if (!m_inputVideoStream) {
        if (error) {
            *error = "receive bypassed hardware frame failed: input stream is null";
        }
        return -1;
    }

    while (!m_bypassedHardwareFrames.empty()) {
        AVFrame* queued = m_bypassedHardwareFrames.front();
        m_bypassedHardwareFrames.pop_front();

        av_frame_unref(frame);
        av_frame_move_ref(frame, queued);
        av_frame_free(&queued);

        const bool ok = rescaleAndValidateFramePts(
            frame,
            m_inputVideoStream->time_base,
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
    while (!m_bypassedHardwareFrames.empty()) {
        AVFrame* frame = m_bypassedHardwareFrames.front();
        m_bypassedHardwareFrames.pop_front();
        av_frame_free(&frame);
    }
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
                    "[ZC][ENCODE] filtered_frame={} fmt={}, hw_frames_ctx={}, encoder_pix_fmt={}",
                    m_filteredHardwareFrameLogCount,
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
