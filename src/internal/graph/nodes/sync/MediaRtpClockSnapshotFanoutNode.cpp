#include "internal/graph/nodes/sync/MediaRtpClockSnapshotFanoutNode.h"

#include "internal/graph/runtime/buffer/MediaRtpClockGroupBuffer.h"

namespace media::ffmpeg::graph {

MediaRtpClockSnapshotFanoutNode::MediaRtpClockSnapshotFanoutNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaRtpClockSnapshotFanoutNode")
{
}

MediaNodeKind MediaRtpClockSnapshotFanoutNode::staticKind() noexcept
{
    return MediaNodeKind::RtpClockSnapshotFanout;
}

::media::Result<MediaNodeProcessResult> MediaRtpClockSnapshotFanoutNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "clock");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) return processWaiting();
    MediaBufferRef buffer = std::move(*input.value());
    if (!buffer->isEof() &&
        !dynamic_cast<const MediaRtpClockGroupBuffer*>(buffer.get())) {
        return processProgress(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP clock snapshot fanout requires immutable group snapshots")));
    }
    const bool eof = buffer->isEof();
    auto status = pushToAllOutputs(context, buffer);
    return eof ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
