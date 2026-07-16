#include "internal/graph/nodes/video/VideoFrameRateNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"
#include "internal/graph/sync/lineage/MediaVideoLineageDerivation.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <charconv>
#include <optional>
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

::media::Result<std::optional<int>> parseIntOption(const MediaNodeOptions* options, const std::string& key)
{
    if (!options) {
        return ::media::Result<std::optional<int>>::success(std::nullopt);
    }

    const std::string value = options->value(key);
    if (value.empty()) {
        return ::media::Result<std::optional<int>>::success(std::nullopt);
    }

    int parsed = 0;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        return ::media::Result<std::optional<int>>::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode invalid integer option: " + key));
    }

    return ::media::Result<std::optional<int>>::success(parsed);
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
    , m_state(std::make_shared<MediaVideoFrameRateState>(false))
{
}

VideoFrameRateNode::VideoFrameRateNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaVideoFrameRateState> state)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFrameRateNode")
    , m_state(std::move(state))
    , m_exposesGenerationPurgeTarget(true)
{
}

MediaNodeKind VideoFrameRateNode::staticKind() noexcept
{
    return MediaNodeKind::VideoFrameRate;
}

std::string_view VideoFrameRateNode::generationPurgeIdentity() noexcept
{
    return "video_frame_rate";
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
VideoFrameRateNode::generationPurgeTarget() const noexcept
{
    return m_exposesGenerationPurgeTarget ? m_state : nullptr;
}

bool VideoFrameRateNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto lineage = FFmpegFrameView::canonicalLineage(buffer);
    return m_state->pendingOutputIsCurrent(
        buffer, lineage ? std::optional<std::uint64_t>(lineage->generation)
                        : std::nullopt);
}

::media::Status VideoFrameRateNode::start(MediaGraphExecutionContext& context)
{
    auto guard = m_state->lock();
    resetRuntimeState();
    return FFmpegNodeRuntime::start(context);
}

::media::Status VideoFrameRateNode::stop(MediaGraphExecutionContext& context)
{
    auto guard = m_state->lock();
    auto status = FFmpegNodeRuntime::stop(context);
    resetRuntimeState();
    return status;
}

void VideoFrameRateNode::abort(MediaGraphExecutionContext& context) noexcept
{
    auto guard = m_state->lock();
    FFmpegNodeRuntime::abort(context);
    resetRuntimeState();
}

void VideoFrameRateNode::resetRuntimeState() noexcept
{
    m_state->resetLifecycle();
}

::media::Result<MediaNodeProcessResult> VideoFrameRateNode::onProcess(MediaGraphExecutionContext& context)
{
    auto stateGuard = m_state->lock();
    auto& state = m_state->data();
    if (state.terminalPending) {
        return continueTerminal(context);
    }
    const bool hadPendingOutput = !state.pendingFrames.empty();
    auto pendingDrain = drainPending(context);
    if (!pendingDrain) return processProgress(std::move(pendingDrain));
    if (hadPendingOutput) return processProgress();
    if (state.terminals.finished()) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        MediaChannel* frameInput = context.findInputChannel(nodeId(), "frame");
        if (frameInput && frameInput->closed()) {
            state.terminals.markClosed("frame");
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        const bool hadPendingFrames = !state.pendingFrames.empty();
        auto drainStatus = drainPending(context);
        if (!drainStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(drainStatus.error());
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            hadPendingFrames ? MediaNodeProcessResult::progress() : MediaNodeProcessResult::waiting());
    }

    MediaBufferRef buffer = *input.value();
    if (buffer->isEof() || buffer->isFlush()) {
        const bool eof = buffer->isEof();
        if (eof && state.eofEmitted) {
            return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::finished());
        }
        state.flushed = true;
        state.terminalBuffer = buffer;
        state.terminalPending = true;
        state.terminalIsEof = eof;
        return continueTerminal(context);
    }

    auto lineage = FFmpegFrameView::canonicalLineage(buffer);
    if (m_state->requiresCanonicalLineage() && !lineage) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "VideoFrameRateNode requires canonical lineage"));
    }
    if (lineage) {
        if (auto status = m_state->activateGeneration(lineage->generation); !status) {
            return ::media::Result<MediaNodeProcessResult>::failure(status.error());
        }
    }

    if (!state.initialized) {
        auto initStatus = initializeFromFirstFrame(context, buffer);
        if (!initStatus) {
            return ::media::Result<MediaNodeProcessResult>::failure(initStatus.error());
        }
    }

    auto sendStatus = sendFrame(context, buffer);
    if (!sendStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(sendStatus.error());
    }

    auto drainStatus = drainPending(context);
    if (!drainStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(drainStatus.error());
    }
    return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
}

::media::Result<MediaNodeProcessResult> VideoFrameRateNode::continueTerminal(
    MediaGraphExecutionContext& context)
{
    auto drainStatus = drainPending(context);
    if (!drainStatus) return processProgress(std::move(drainStatus));
    auto& state = m_state->data();
    const bool eof = state.terminalIsEof;
    MediaBufferRef terminal = std::move(state.terminalBuffer);
    state.terminalPending = false;
    state.terminalIsEof = false;
    if (eof) {
        state.terminals.markEof("frame");
        state.eofEmitted = true;
    }
    if (auto freshness = m_state->authorizeRetainedControl(terminal);
        !freshness) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            freshness.error());
    }
    auto broadcastStatus = broadcastControlToAllOutputs(context, terminal);
    return eof ? processFinished(std::move(broadcastStatus))
               : processProgress(std::move(broadcastStatus));
}

::media::Status VideoFrameRateNode::initializeFromFirstFrame(MediaGraphExecutionContext& context,
                                                             const MediaBufferRef& buffer)
{
    auto& state = m_state->data();
    const MediaRational inputTimeBase = buffer ? buffer->timeDescriptor().timeBase : MediaRational{};
    if (!inputTimeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode requires input frame time_base"));
    }

    const MediaNodeOptions* options = nodeOptions(context);
    auto fpsNumOption = parseIntOption(options, MediaTranscodeOptionKey::VideoFpsNum);
    if (!fpsNumOption) {
        return ::media::Status::failure(fpsNumOption.error());
    }
    auto fpsDenOption = parseIntOption(options, MediaTranscodeOptionKey::VideoFpsDen);
    if (!fpsDenOption) {
        return ::media::Status::failure(fpsDenOption.error());
    }

    const int fpsNum = fpsNumOption.value().value_or(0);
    const int fpsDen = fpsDenOption.value().value_or(1);
    if (fpsNum < 0 || fpsDen <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode target fps is invalid"));
    }

    state.inputTimeBase = toAVRational(inputTimeBase);
    state.targetFramePeriod = fpsNum > 0 ? AVRational{ fpsDen, fpsNum } : AVRational{ 0, 1 };
    state.initialized = true;

    std::ostringstream out;
    out << "initialize input_tb=" << rationalText(state.inputTimeBase)
        << " target_fps=" << (fpsNum > 0 ? std::to_string(fpsNum) + "/" + std::to_string(fpsDen) : std::string("passthrough"))
        << " mode=" << (enabled() ? "control" : "passthrough");
    frameRateLog(MediaGraphDiagnosticLevel::State, out.str());
    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::sendFrame(MediaGraphExecutionContext&, const MediaBufferRef& buffer)
{
    auto& state = m_state->data();
    if (!state.initialized) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("VideoFrameRateNode is not initialized"));
    }

    if (state.flushed) {
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
        auto lineage = FFmpegFrameView::canonicalLineage(buffer);
        state.pendingFrames.push_back(
            {buffer, lineage ? lineage->generation : 0});
        return ::media::Status::success();
    }

    const int64_t currentPts = frame->pts;
    if (!state.started) {
        state.started = true;
        state.startPts = currentPts;
        state.nextOutputIndex = 0;
    } else if (currentPts < state.lastInputPts) {
        std::ostringstream out;
        out << "VideoFrameRateNode input pts moved backwards current=" << currentPts
            << " last=" << state.lastInputPts;
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    int64_t queued = 0;
    while (true) {
        const int64_t targetPts = targetPtsForIndex(state.nextOutputIndex);
        if (targetPts > currentPts) {
            break;
        }

        const AVFrame* sourceFrame = chooseSourceFrameForTarget(frame, currentPts, targetPts);
        if (!sourceFrame) {
            sourceFrame = frame;
        }

        auto selectedLineage = sourceFrame == frame
            ? FFmpegFrameView::canonicalLineage(buffer)
            : FFmpegFrameView::canonicalLineage(state.lastInputFrame.buffer);
        auto queueStatus = queueFrameReference(sourceFrame, targetPts,
                                                std::move(selectedLineage));
        if (!queueStatus) {
            return queueStatus;
        }

        ++state.nextOutputIndex;
        ++queued;
    }

    auto rememberStatus = rememberLastInputFrame(buffer);
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
            << " next_index=" << state.nextOutputIndex;
        frameRateLog(MediaGraphDiagnosticLevel::Flow, out.str());
    }

    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::drainPending(MediaGraphExecutionContext& context)
{
    auto& state = m_state->data();
    while (!state.pendingFrames.empty()) {
        const MediaBufferRef pendingFrame = state.pendingFrames.front().buffer;
        auto emitStatus = emitOutput(context, "frame", pendingFrame);
        if (!emitStatus) {
            if (retainsPendingOutput(pendingFrame)) {
                state.pendingFrames.pop_front();
            }
            return emitStatus;
        }
        state.pendingFrames.pop_front();
    }

    return ::media::Status::success();
}

::media::Status VideoFrameRateNode::queueFrameReference(
    const AVFrame* sourceFrame, int64_t outputPts,
    std::shared_ptr<const MediaCanonicalLineage> lineage)
{
    auto& state = m_state->data();
    if (!sourceFrame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode source frame is null"));
    }

    if (state.lastOutputPts != AV_NOPTS_VALUE && outputPts <= state.lastOutputPts) {
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
    timeDescriptor.timeBase = toMediaRational(state.inputTimeBase);
    timeDescriptor.frameRate = enabled() ? MediaRational{ state.targetFramePeriod.den, state.targetFramePeriod.num }
                                         : MediaRational{};
    cloned.value()->setTimeDescriptor(timeDescriptor);
    cloned.value()->setTimestamps(outputFrame->pts, outputFrame->pkt_dts, outputFrame->duration);
    MediaBufferRef output = cloned.value();
    if (lineage) {
        auto derived = deriveMediaVideoLineage(
            *lineage, sourceFrame->pts, outputPts, outputFrame->duration,
            toMediaRational(state.inputTimeBase));
        if (!derived) return ::media::Status::failure(derived.error());
        auto canonical = MediaCanonicalVideoFrameBuffer::create(
            output, std::move(derived).value());
        if (!canonical) return ::media::Status::failure(canonical.error());
        output = std::move(canonical).value();
    }
    state.pendingFrames.push_back(
        {std::move(output), lineage ? lineage->generation : 0});
    state.lastOutputPts = outputPts;
    return ::media::Status::success();
}

int64_t VideoFrameRateNode::targetPtsForIndex(int64_t index) const noexcept
{
    const auto& state = m_state->data();
    return av_rescale_q(index, state.targetFramePeriod, state.inputTimeBase);
}

int64_t VideoFrameRateNode::targetFrameDuration() const noexcept
{
    if (!enabled()) {
        return 0;
    }
    const auto& state = m_state->data();
    return av_rescale_q(1, state.targetFramePeriod, state.inputTimeBase);
}

bool VideoFrameRateNode::enabled() const noexcept
{
    return rationalKnown(m_state->data().targetFramePeriod);
}

const AVFrame* VideoFrameRateNode::chooseSourceFrameForTarget(const AVFrame* frame, int64_t currentPts, int64_t targetPts) const noexcept
{
    const auto& state = m_state->data();
    const AVFrame* previousFrame = FFmpegFrameView::frame(
        state.lastInputFrame.buffer);
    if (!previousFrame || state.lastInputPts == AV_NOPTS_VALUE) {
        return frame;
    }

    const int64_t previousDistance = absoluteDistance(targetPts, state.lastInputPts);
    const int64_t currentDistance = absoluteDistance(currentPts, targetPts);
    return previousDistance <= currentDistance ? previousFrame : frame;
}

::media::Status VideoFrameRateNode::rememberLastInputFrame(const MediaBufferRef& buffer)
{
    auto& state = m_state->data();
    const AVFrame* frame = FFmpegFrameView::frame(buffer);
    if (!frame) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("VideoFrameRateNode expected frame buffer for history"));
    }

    auto cloned = FFmpegBufferFactory::cloneFrame(frame, MediaStreamKind::Video);
    if (!cloned) {
        return ::media::Status::failure(cloned.error());
    }

    MediaBufferRef remembered = cloned.value();
    if (auto lineage = FFmpegFrameView::canonicalLineage(buffer)) {
        auto canonical = MediaCanonicalVideoFrameBuffer::create(remembered, std::move(lineage));
        if (!canonical) return ::media::Status::failure(canonical.error());
        remembered = std::move(canonical).value();
    }
    auto lineage = FFmpegFrameView::canonicalLineage(buffer);
    state.lastInputFrame = {
        std::move(remembered), lineage ? lineage->generation : 0};
    state.lastInputPts = frame->pts;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
