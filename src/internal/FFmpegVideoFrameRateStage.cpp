#include "internal/FFmpegVideoFrameRateStage.h"

#include "internal/FFmpegUtils.h"

#include <cstdlib>
#include <sstream>
#include <utility>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

namespace media::ffmpeg {
namespace {

bool isValidTimeBase(AVRational timeBase)
{
    return timeBase.num > 0 && timeBase.den > 0;
}

int64_t absoluteDistance(int64_t left, int64_t right)
{
    return left >= right ? left - right : right - left;
}

} // namespace

FFmpegVideoFrameRateStage::~FFmpegVideoFrameRateStage()
{
    reset();
}

void FFmpegVideoFrameRateStage::reset()
{
    m_pendingFrames.clear();
    m_lastInputFrame.reset();

    m_inputTimeBase = AVRational{ 0, 1 };
    m_targetFps = 0;
    m_initialized = false;
    m_started = false;
    m_flushed = false;

    m_startPts = 0;
    m_nextOutputIndex = 0;
    m_lastInputPts = 0;
    m_lastOutputPts = AV_NOPTS_VALUE;
}

bool FFmpegVideoFrameRateStage::initialize(const Config& config, std::string* error)
{
    reset();

    if (!isValidTimeBase(config.inputTimeBase)) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage initialize failed: input time base is invalid";
        }
        return false;
    }

    if (config.targetFps < 0) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage initialize failed: target fps is invalid";
        }
        return false;
    }

    m_inputTimeBase = config.inputTimeBase;
    m_targetFps = config.targetFps;
    m_initialized = true;
    return true;
}

bool FFmpegVideoFrameRateStage::sendFrame(AVFrame* frame, std::string* error)
{
    if (!m_initialized) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage sendFrame failed: stage is not initialized";
        }
        return false;
    }

    if (m_flushed) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage sendFrame failed: stage is already flushed";
        }
        return false;
    }

    if (!frame) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage sendFrame failed: frame is null";
        }
        return false;
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage sendFrame failed: frame pts is invalid";
        }
        return false;
    }

    if (!enabled()) {
        return queuePassthroughFrame(frame, error);
    }

    const int64_t currentPts = frame->pts;

    if (!m_started) {
        m_started = true;
        m_startPts = currentPts;
        m_nextOutputIndex = 0;
    }
    else if (currentPts < m_lastInputPts) {
        if (error) {
            std::ostringstream oss;
            oss << "FFmpegVideoFrameRateStage sendFrame failed: input pts moved backwards, current="
                << currentPts
                << ", last="
                << m_lastInputPts;
            *error = oss.str();
        }
        return false;
    }

    while (true) {
        const int64_t targetPts = targetPtsForIndex(m_nextOutputIndex);
        if (targetPts > currentPts) {
            break;
        }

        AVFrame* sourceFrame = chooseSourceFrameForTarget(frame, currentPts, targetPts);
        if (!sourceFrame) {
            sourceFrame = frame;
        }

        if (!queueFrameReference(sourceFrame, targetPts, error)) {
            return false;
        }

        ++m_nextOutputIndex;
    }

    return rememberLastInputFrame(frame, error);
}

bool FFmpegVideoFrameRateStage::flush(std::string* error)
{
    if (!m_initialized) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage flush failed: stage is not initialized";
        }
        return false;
    }

    m_flushed = true;
    return true;
}

int FFmpegVideoFrameRateStage::receiveFrame(AVFrame* frame, std::string* error)
{
    if (!m_initialized) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage receiveFrame failed: stage is not initialized";
        }
        return -1;
    }

    if (!frame) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage receiveFrame failed: frame is null";
        }
        return -1;
    }

    if (m_pendingFrames.empty()) {
        return 0;
    }

    FramePtr nextFrame = std::move(m_pendingFrames.front());
    m_pendingFrames.pop_front();

    av_frame_unref(frame);
    av_frame_move_ref(frame, nextFrame.get());
    return 1;
}

bool FFmpegVideoFrameRateStage::isInitialized() const
{
    return m_initialized;
}

bool FFmpegVideoFrameRateStage::enabled() const
{
    return m_targetFps > 0;
}

AVRational FFmpegVideoFrameRateStage::inputTimeBase() const
{
    return m_inputTimeBase;
}

int FFmpegVideoFrameRateStage::targetFps() const
{
    return m_targetFps;
}

int64_t FFmpegVideoFrameRateStage::targetPtsForIndex(int64_t outputIndex) const
{
    return m_startPts + av_rescale_q(
        outputIndex,
        AVRational{ 1, m_targetFps },
        m_inputTimeBase
    );
}

int64_t FFmpegVideoFrameRateStage::targetFrameDuration() const
{
    if (!enabled()) {
        return 0;
    }

    const int64_t duration = av_rescale_q(
        1,
        AVRational{ 1, m_targetFps },
        m_inputTimeBase
    );

    return duration > 0 ? duration : 1;
}

bool FFmpegVideoFrameRateStage::queueFrameReference(AVFrame* sourceFrame,
                                                    int64_t outputPts,
                                                    std::string* error)
{
    if (!sourceFrame) {
        if (error) {
            *error = "FFmpegVideoFrameRateStage queueFrameReference failed: source frame is null";
        }
        return false;
    }

    if (m_lastOutputPts != AV_NOPTS_VALUE && outputPts <= m_lastOutputPts) {
        return true;
    }

    FramePtr queuedFrame = makeFrame();
    if (!queuedFrame) {
        if (error) {
            *error = "av_frame_alloc frame-rate output frame failed";
        }
        return false;
    }

    const int ret = av_frame_ref(queuedFrame.get(), sourceFrame);
    if (ret < 0) {
        if (error) {
            *error = "av_frame_ref frame-rate output frame failed: " + errorString(ret);
        }
        return false;
    }

    queuedFrame->pts = outputPts;
    queuedFrame->duration = targetFrameDuration();
    m_lastOutputPts = outputPts;
    m_pendingFrames.push_back(std::move(queuedFrame));
    return true;
}

bool FFmpegVideoFrameRateStage::queuePassthroughFrame(AVFrame* frame, std::string* error)
{
    FramePtr queuedFrame = makeFrame();
    if (!queuedFrame) {
        if (error) {
            *error = "av_frame_alloc frame-rate passthrough frame failed";
        }
        return false;
    }

    const int ret = av_frame_ref(queuedFrame.get(), frame);
    if (ret < 0) {
        if (error) {
            *error = "av_frame_ref frame-rate passthrough frame failed: " + errorString(ret);
        }
        return false;
    }

    m_pendingFrames.push_back(std::move(queuedFrame));
    return true;
}

bool FFmpegVideoFrameRateStage::rememberLastInputFrame(AVFrame* frame, std::string* error)
{
    FramePtr rememberedFrame = makeFrame();
    if (!rememberedFrame) {
        if (error) {
            *error = "av_frame_alloc frame-rate last input frame failed";
        }
        return false;
    }

    const int ret = av_frame_ref(rememberedFrame.get(), frame);
    if (ret < 0) {
        if (error) {
            *error = "av_frame_ref frame-rate last input frame failed: " + errorString(ret);
        }
        return false;
    }

    m_lastInputPts = frame->pts;
    m_lastInputFrame = std::move(rememberedFrame);
    return true;
}

AVFrame* FFmpegVideoFrameRateStage::chooseSourceFrameForTarget(AVFrame* currentFrame,
                                                               int64_t currentPts,
                                                               int64_t targetPts) const
{
    if (!m_lastInputFrame) {
        return currentFrame;
    }

    const int64_t currentDistance = absoluteDistance(currentPts, targetPts);
    const int64_t lastDistance = absoluteDistance(m_lastInputPts, targetPts);

    return currentDistance < lastDistance ? currentFrame : m_lastInputFrame.get();
}

} // namespace media::ffmpeg
