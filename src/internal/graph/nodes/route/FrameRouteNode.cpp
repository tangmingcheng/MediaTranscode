#include "internal/graph/nodes/route/FrameRouteNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

FrameRouteNode::FrameRouteNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FrameRouteNode")
{
}

MediaNodeKind FrameRouteNode::staticKind() noexcept
{
    return MediaNodeKind::FrameRoute;
}

::media::Result<MediaNodeProcessResult> FrameRouteNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    auto status = pushToAllOutputs(context, *input.value());
    return (*input.value())->isEof() ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
