#include "internal/graph/nodes/video/HardwareTransferNode.h"

namespace media::ffmpeg::graph {

HardwareTransferNode::HardwareTransferNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "HardwareTransferNode")
{
}

MediaNodeKind HardwareTransferNode::staticKind() noexcept
{
    return MediaNodeKind::HardwareTransfer;
}

::media::Status HardwareTransferNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
