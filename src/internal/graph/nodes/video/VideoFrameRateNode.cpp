#include "internal/graph/nodes/video/VideoFrameRateNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <cstdlib>
#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

bool rationalKnown(AVRational rational) noexcept
{
    return rational.num > 0 && rational.den > 0;
}

AVRational toAVRational(MediaRational rational) noexcept
{
    return AVRational{ rational.num, rational.den };
}

MediaRational toMediaRational(AVRational rational) noexcept
{
    return MediaRational{ rational.num, rational.den };
}

int parseIntOption(const MediaNodeOptions* options, const std::string& key, int fallback)
{
    if (!options) {
        return fallback;
    }

    const std::string value = options->value(key);
    if (value.empty()) {
        return fallback;
    }

    return std::atoi(value.c_str());
}

int64_t absoluteDistance(int64_t left, int64_t right) noexcept
{
    return left >= right ? left - right : right - left;
}

std::string rationalText(AVRational rational)
{
    if (!rationalKnown(rational)) {
        return "disabled";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

void frameRateLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("video_framerate.") + message);
}

} // namespace

VideoFrameRateNode::VideoFrameRateNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFrameRateNode")
{
}

MediaNodeKind VideoFrameRateNode::staticKind() noexcept
{
    return MediaNodeKind::VideoFrameRate;
}

::media::Status VideoFrameRateNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInput(context);
    if (!input) {
        return drainPending(context);
    }

    MediaBufferRef buffer = input.value();
    if (buffer->isEof() || buffer->isFlush()) {
        m_flushed = true;
        auto drainStatus = drainPending(context);
        if (!drainStatus) {
            return drainStatus;
        }
        return pushToAllOutputs(context, buffer);
    }

    if (!m_initialized) {
        auto initStatus = initializeFromFirstFrame(context, buffer);
        if (!initStatus) {
            return initStatus;
        }
    }

    auto sendStatus = sendFrame(context, buffer);
    if (!sendStatus) {
        return sendStatus;
    }

    return drainPending(context);
}

::media::Status VideoFrameRateNode::initializeFromFirstFrame(MediaGraphExecutionContext& context,
                                                             const MediaBufferRef& buffer)
{
    const MediaRational inputTimeBase = buffer ? buffer->timeDescriptor().timeBase : MediaRational{};
    if (!inputTimeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode requires input frame time_base"));
    }

    const MediaNodeOptions* options = nodeOptions(context);
    const int fpsNum = parseIntOption(options, "fps_num", 0);
    const int fpsDen = parseIntOption(options, "fps_den", 1);
    if (fpsNum < 0 || fpsDen <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode target fps is invalid"));
    }

    m_inputTimeBase = toAVRational(inputTimeBase);
    m_targetFramePeriod = fpsNum > 0 ? AVRational{ fpsDen, fpsNum } : AVRational{ 0, 1 };
    m_initialized = true;

    std::ostringstream out;
    out << "initialize input_tb=" << rationalText(m_inputTimeBase)
        << " target_fps=" << (fpsNum > 0 ? std::to_string(fpsNum) + "/" + std::to_string(fpsDen) : std::string("passthrough"))
        << " mode=" << (enabled() ? "control" : "passthrough");
    frameRateLog(MediaGraphDiagnosticLevel::State, out.str());
    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::sendFrame(MediaGraphExecutionContext&, const MediaBufferRef& buffer)
{
    if (!m_initialized) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFrameRateNode is not initialized"));
    }

    if (m_flushed) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode received frame after flush"));
    }

    const AVFrame* frame = FFmpegFrameView::frame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode expected frame buffer"));
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode requires frame pts"));
    }

    if (!enabled()) {
        m_pendingFrames.push_back(buffer);
        return ::media::Status::success();
    }

    const int64_t currentPts = frame->pts;
    if (!m_started) {
        m_started = true;
        m_startPts = currentPts;
        m_nextOutputIndex = 0;
    } else if (currentPts < m_lastInputPts) {
        std::ostringstream out;
        out << "VideoFrameRateNode input pts moved backwards current=" << currentPts
            << " last=" << m_lastInputPts;
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    int64_t queued = 0;
    while (true) {
        const int64_t targetPts = targetPtsForIndex(m_nextOutputIndex);
        if (targetPts > currentPts) {
            break;
        }

        const AVFrame* sourceFrame = chooseSourceFrameForTarget(frame, currentPts, targetPts);
        if (!sourceFrame) {
            sourceFrame = frame;
        }

        auto queueStatus = queueFrameReference(sourceFrame, targetPts);
        if (!queueStatus) {
            return queueStatus;
        }

        ++m_nextOutputIndex;
        ++queued;
    }

    auto rememberStatus = rememberLastInputFrame(frame);
    if (!rememberStatus) {
        return rememberStatus;
    }

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow,
                                               "video_framerate.send_frame");
    if (decision.shouldLog) {
        std::ostringstream out;
        out << "send_frame seq=" << decision.sequence
            << " input_pts=" << currentPts
            << " queued=" << queued
            << " next_index=" << m_nextOutputIndex;
        frameRateLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::drainPending(MediaGraphExecutionContext& context)
{
    while (!m_pendingFrames.empty()) {
        MediaBufferRef frame = std::move(m_pendingFrames.front());
        m_pendingFrames.pop_front();

        auto pushStatus = pushOutput(context, "frame", frame);
        if (!pushStatus) {
            return pushStatus;
        }
    }

    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::queueFrameReference(const AVFrame* sourceFrame, int64_t outputPts)
{
    if (!sourceFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode source frame is null"));
    }

    if (m_lastOutputPts != AV_NOPTS_VALUE && outputPts <= m_lastOutputPts) {
        return ::media::Status::success();
    }

    auto cloned = FFmpegBufferFactory::cloneFrame(sourceFrame, MediaStreamKind::Video);
    if (!cloned) {
        return ::media::Status::failure(cloned.error());
    }

    AVFrame* outputFrame = FFmpegFrameView::writableFrame(cloned.value());
    if (!outputFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode cloned frame is invalid"));
    }

    outputFrame->pts = outputPts;
    outputFrame->duration = targetFrameDuration();

    MediaTimeDescriptor timeDescriptor;
    timeDescriptor.timeBase = toMediaRational(m_inputTimeBase);
    if (enabled()) {
        timeDescriptor.frameRate = MediaRational{ m_targetFramePeriod.den, m_targetFramePeriod.num };
    }
    cloned.value()->setTimeDescriptor(timeDescriptor);
    cloned.value()->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);

    m_lastOutputPts = outputPts;
    m_pendingFrames.push_back(std::move(cloned).value());
    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::rememberLastInputFrame(const AVFrame* frame)
{
    auto remembered = FFmpegBufferFactory::cloneFrame(frame, MediaStreamKind::Video);
    if (!remembered) {
        return ::media::Status::failure(remembered.error());
    }

    m_lastInputPts = frame->pts;
    m_lastInputFrame = std::move(remembered).value();
    return ::media::Status::success();
}

int64_t VideoFrameRateNode::targetPtsForIndex(int64_t outputIndex) const noexcept
{
    return m_startPts + av_rescale_q(outputIndex, m_targetFramePeriod, m_inputTimeBase);
}

int64_t VideoFrameRateNode::targetFrameDuration() const noexcept
{
    if (!enabled()) {
        return 0;
    }

    const int64_t duration = av_rescale_q(1, m_targetFramePeriod, m_inputTimeBase);
    return duration > 0 ? duration : 1;
}

const AVFrame* VideoFrameRateNode::chooseSourceFrameForTarget(const AVFrame* currentFrame,
                                                              int64_t currentPts,
                                                              int64_t targetPts) const noexcept
{
    const AVFrame* lastFrame = FFmpegFrameView::frame(m_lastInputFrame);
    if (!lastFrame) {
        return currentFrame;
    }

    const int64_t currentDistance = absoluteDistance(currentPts, targetPts);
    const int64_t lastDistance = absoluteDistance(m_lastInputPts, targetPts);
    return currentDistance < lastDistance ? currentFrame : lastFrame;
}

bool VideoFrameRateNode::enabled() const noexcept
{
    return rationalKnown(m_targetFramePeriod);
}

} // namespace media::ffmpeg::graph
