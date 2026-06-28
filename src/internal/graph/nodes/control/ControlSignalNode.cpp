#include "internal/graph/nodes/control/ControlSignalNode.h"

namespace media::ffmpeg::graph {

ControlSignalNode::ControlSignalNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "ControlSignalNode")
{
}

MediaNodeKind ControlSignalNode::staticKind() noexcept
{
    return MediaNodeKind::ControlSignal;
}

::media::Status ControlSignalNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
