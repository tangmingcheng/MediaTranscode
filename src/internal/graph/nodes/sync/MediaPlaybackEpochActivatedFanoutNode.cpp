#include "internal/graph/nodes/sync/MediaPlaybackEpochActivatedFanoutNode.h"

#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"

namespace media::ffmpeg::graph {

MediaPlaybackEpochActivatedFanoutNode::MediaPlaybackEpochActivatedFanoutNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaPlaybackEpochActivatedFanoutNode")
{
}

MediaNodeKind MediaPlaybackEpochActivatedFanoutNode::staticKind() noexcept
{
    return MediaNodeKind::PlaybackEpochActivatedFanout;
}

::media::Result<MediaNodeProcessResult>
MediaPlaybackEpochActivatedFanoutNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "activated");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) return processWaiting();
    MediaBufferRef event = std::move(*input.value());
    if (!dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(event.get())) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch fanout requires a typed activation event"));
    }
    return processProgress(pushToAllOutputs(context, event));
}

} // namespace media::ffmpeg::graph
