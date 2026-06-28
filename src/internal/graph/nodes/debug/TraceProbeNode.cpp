#include "internal/graph/nodes/debug/TraceProbeNode.h"

namespace media::ffmpeg::graph {

TraceProbeNode::TraceProbeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "TraceProbeNode")
{
}

MediaNodeKind TraceProbeNode::staticKind() noexcept
{
    return MediaNodeKind::TraceProbe;
}

::media::Status TraceProbeNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
