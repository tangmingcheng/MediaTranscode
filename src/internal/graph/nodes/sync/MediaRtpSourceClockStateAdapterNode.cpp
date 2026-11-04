#include "internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h"

#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/model/MediaTranscodeStreamSetCodec.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"

namespace media::ffmpeg::graph {

MediaRtpSourceClockStateAdapterNode::MediaRtpSourceClockStateAdapterNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaRtpSourceClockStateAdapterNode")
{
}

MediaNodeKind MediaRtpSourceClockStateAdapterNode::staticKind() noexcept
{
    return MediaNodeKind::RtpSourceClockStateAdapter;
}

::media::Status MediaRtpSourceClockStateAdapterNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto option = requiredNodeOption(nodeOptions(context),
        "MediaRtpSourceClockStateAdapterNode", "rtp_source_clock.members");
    if (!option) return ::media::Status::failure(option.error());
    auto members = MediaTranscodeStreamSetCodec::decode(option.value());
    if (!members) return ::media::Status::failure(members.error());
    m_members = members.value();
    return FFmpegNodeRuntime::start(context);
}

::media::Result<MediaNodeProcessResult>
MediaRtpSourceClockStateAdapterNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (!m_members) return processProgress(::media::Status::failure(
        ::media::ErrorInfo::notInitialized("RTP source clock adapter requires planned members")));
    if (m_pendingState) return emitPendingState(context);

    auto input = tryReadRequiredInput(
        context.findInputChannel(nodeId(), "clock"),
        "RTP source-clock state adapter", "clock");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) return processWaiting();
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            input.value()->get())) {
        if (control->controlKind() == MediaControlBufferKind::Unknown) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP source-clock state adapter rejects unknown control"));
        }
        auto terminal = broadcastControlToAllOutputs(context, *input.value());
        return control->controlKind() == MediaControlBufferKind::Eof ||
                       control->controlKind() == MediaControlBufferKind::Abort
            ? processFinished(std::move(terminal))
            : processProgress(std::move(terminal));
    }
    const auto* clock = dynamic_cast<const MediaRtpClockGroupBuffer*>(
        input.value()->get());
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP source clock adapter requires a locked-group snapshot"));
    }
    const auto& snapshot = clock->snapshot();
    auto projected = projectMediaRtpSourceClockState(snapshot, *m_members);
    if (!projected) return processProgress(::media::Status::failure(projected.error()));
    const auto& projection = projected.value();
    if (m_lastEmittedProjection == projection) return processProgress();

    m_pendingProjection = projection;
    m_pendingState = makeMediaBufferRef<MediaSourceClockStateBuffer>(
        projection.readiness, projection.generation, projection.discontinuity, projection.evidenceRevision);
    return emitPendingState(context);
}

::media::Result<MediaNodeProcessResult>
MediaRtpSourceClockStateAdapterNode::emitPendingState(
    MediaGraphExecutionContext& context)
{
    auto emitted = emitOutput(context, "state", m_pendingState);
    if (!emitted) {
        return emitted.error().code == ::media::ErrorCode::WouldBlock
            ? processWaiting()
            : ::media::Result<MediaNodeProcessResult>::failure(
                  emitted.error());
    }
    m_lastEmittedProjection = m_pendingProjection;
    m_pendingProjection.reset();
    m_pendingState.reset();
    return processProgress();
}

::media::Status MediaRtpSourceClockStateAdapterNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaRtpSourceClockStateAdapterNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaRtpSourceClockStateAdapterNode::resetState() noexcept
{
    m_members.reset();
    m_lastEmittedProjection.reset();
    m_pendingProjection.reset();
    m_pendingState.reset();
}

} // namespace media::ffmpeg::graph
