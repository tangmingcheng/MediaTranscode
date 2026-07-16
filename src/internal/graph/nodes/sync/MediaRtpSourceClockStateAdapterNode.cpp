#include "internal/graph/nodes/sync/MediaRtpSourceClockStateAdapterNode.h"

#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"
#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"

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

::media::Result<MediaNodeProcessResult>
MediaRtpSourceClockStateAdapterNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "clock");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) return processWaiting();
    const auto* clock = dynamic_cast<const MediaRtpClockGroupBuffer*>(
        input.value()->get());
    if (!clock) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP source clock adapter requires a locked-group snapshot"));
    }
    const auto& snapshot = clock->snapshot();
    MediaSourceClockReadiness readiness;
    bool discontinuity = false;
    switch (snapshot.state) {
    case MediaRtpClockGroupState::Acquiring:
        readiness = MediaSourceClockReadiness::Acquiring;
        break;
    case MediaRtpClockGroupState::Locked:
        if (snapshot.groupGeneration == 0 || !snapshot.locked) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Locked RTP clock snapshot is incomplete"));
        }
        readiness = MediaSourceClockReadiness::Locked;
        break;
    case MediaRtpClockGroupState::Degraded:
        readiness = MediaSourceClockReadiness::Degraded;
        break;
    case MediaRtpClockGroupState::ReacquireRequired:
        readiness = MediaSourceClockReadiness::ReacquireRequired;
        discontinuity = true;
        break;
    }
    auto adapted = makeMediaBufferRef<MediaSourceClockStateBuffer>(
        readiness, snapshot.groupGeneration, discontinuity);
    return processProgress(emitOutput(context, "state", adapted));
}

} // namespace media::ffmpeg::graph
