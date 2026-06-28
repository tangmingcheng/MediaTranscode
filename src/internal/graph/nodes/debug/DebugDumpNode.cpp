#include "internal/graph/nodes/debug/DebugDumpNode.h"

namespace media::ffmpeg::graph {

DebugDumpNode::DebugDumpNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "DebugDumpNode")
{
}

MediaNodeKind DebugDumpNode::staticKind() noexcept
{
    return MediaNodeKind::DebugDump;
}

::media::Status DebugDumpNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
