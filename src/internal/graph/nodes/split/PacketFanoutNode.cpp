#include "internal/graph/nodes/split/PacketFanoutNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

PacketFanoutNode::PacketFanoutNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketFanoutNode")
{
}

MediaNodeKind PacketFanoutNode::staticKind() noexcept
{
    return MediaNodeKind::PacketFanout;
}

::media::Result<MediaNodeProcessResult> PacketFanoutNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    auto status = pushToAllOutputs(context, *input.value());
    return (*input.value())->isEof() ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
