#include "internal/graph/nodes/sync/MediaSourceClockStateFanoutNode.h"

#include "internal/graph/runtime/buffer/MediaSourceClockStateBuffer.h"

namespace media::ffmpeg::graph {

MediaSourceClockStateFanoutNode::MediaSourceClockStateFanoutNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaSourceClockStateFanoutNode")
{
}

MediaNodeKind MediaSourceClockStateFanoutNode::staticKind() noexcept
{
    return MediaNodeKind::SourceClockStateFanout;
}

::media::Result<MediaNodeProcessResult>
MediaSourceClockStateFanoutNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "clock");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) return processWaiting();
    MediaBufferRef buffer = std::move(*input.value());
    if (!buffer->isEof() &&
        !dynamic_cast<const MediaSourceClockStateBuffer*>(buffer.get())) {
        return processProgress(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Source-clock state fanout requires generic clock state")));
    }
    const bool eof = buffer->isEof();
    auto status = pushToAllOutputs(context, buffer);
    return eof ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
