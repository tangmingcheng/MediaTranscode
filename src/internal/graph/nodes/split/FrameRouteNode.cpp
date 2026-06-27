#include "internal/graph/nodes/split/FrameRouteNode.h"

namespace media::ffmpeg::graph {

FrameRouteNode::FrameRouteNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FrameRouteNode")
{
}

MediaNodeKind FrameRouteNode::staticKind() noexcept
{
    return MediaNodeKind::FrameRoute;
}

::media::Status FrameRouteNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
