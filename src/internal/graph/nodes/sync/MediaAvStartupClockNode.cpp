#include "internal/graph/nodes/sync/MediaAvStartupClockNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {

MediaAvStartupClockNode::MediaAvStartupClockNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAvStartupClockNode")
{
}

MediaNodeKind MediaAvStartupClockNode::staticKind() noexcept
{
    return MediaNodeKind::AvStartupClock;
}

::media::Status MediaAvStartupClockNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto groupName = requiredNodeOption(
        nodeOptions(context), "MediaAvStartupClockNode",
        "av_startup_clock.sync_group");
    auto interval = requiredPositiveInt64NodeOption(
        nodeOptions(context), "MediaAvStartupClockNode",
        "av_startup_clock.interval_ns");
    if (!groupName) return ::media::Status::failure(groupName.error());
    if (!interval) return ::media::Status::failure(interval.error());
    m_groupKey.emplace(std::move(groupName).value());
    if (!m_groupKey->valid()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "A/V startup clock requires a valid planned sync group"));
    }
    m_group = context.findAvSyncGroup(*m_groupKey);
    if (!m_group) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "A/V startup clock requires a registered sync group"));
    }
    if (!context.findInputChannel(nodeId(), "clock") ||
        !context.findOutputChannel(nodeId(), "tick")) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "A/V startup clock requires clock input and tick output"));
    }
    m_interval = MediaRunningTime::fromNanoseconds(interval.value());
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaAvStartupClockNode::observe(
    const MediaSourceClockStateBuffer& state)
{
    if (state.readiness() == MediaSourceClockReadiness::Acquiring) {
        if (m_generation) {
            return ::media::Status::failure(::media::ErrorInfo::cancelled(
                "A/V startup clock rejects readiness regression"));
        }
        return ::media::Status::success();
    }
    if (state.readiness() != MediaSourceClockReadiness::Locked ||
        state.generation() == 0) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "A/V startup clock requires locked source-clock state"));
    }
    if (m_generation && *m_generation != state.generation()) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "A/V startup clock rejects generation changes"));
    }
    m_generation = state.generation();
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaAvStartupClockNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto state = tryReadRequiredInput(
        context.findInputChannel(nodeId(), "clock"),
        "A/V startup clock", "clock");
    if (!state) {
        return ::media::Result<MediaNodeProcessResult>::failure(state.error());
    }
    if (state.value()) {
        if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
                state.value()->get())) {
            if (control->controlKind() == MediaControlBufferKind::Unknown) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "A/V startup clock rejects unknown control"));
            }
            if (!m_generation) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::cancelled(
                        "A/V startup clock rejects control before source-clock lock"));
            }
            const bool finished =
                control->controlKind() == MediaControlBufferKind::Eof ||
                control->controlKind() == MediaControlBufferKind::Abort;
            auto emitted = emitOutput(context, "tick", *state.value());
            return finished ? processFinished(std::move(emitted))
                            : processProgress(std::move(emitted));
        }
        const auto* clockState = dynamic_cast<const MediaSourceClockStateBuffer*>(
            state.value()->get());
        if (!clockState) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V startup clock requires generic source-clock state"));
        }
        if (auto observed = observe(*clockState); !observed) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                observed.error());
        }
    }
    if (!m_generation) return processWaiting();
    auto now = m_group->clock()->now();
    if (!now) {
        return ::media::Result<MediaNodeProcessResult>::failure(now.error());
    }
    if (m_nextTick && now.value() < *m_nextTick) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(*m_groupKey, *m_nextTick));
    }
    auto next = now.value().checkedAdd(*m_interval);
    if (!next) {
        return ::media::Result<MediaNodeProcessResult>::failure(next.error());
    }
    m_nextTick = next.value();
    auto tick = makeMediaBufferRef<MediaAvStartupClockBuffer>(now.value());
    return processProgress(emitOutput(context, "tick", tick));
}

::media::Status MediaAvStartupClockNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaAvStartupClockNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaAvStartupClockNode::resetState() noexcept
{
    m_groupKey.reset();
    m_group.reset();
    m_interval.reset();
    m_nextTick.reset();
    m_generation.reset();
}

} // namespace media::ffmpeg::graph
