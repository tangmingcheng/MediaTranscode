#include "internal/graph/nodes/output/RtpOutputNode.h"

namespace media::ffmpeg::graph {

RtpOutputNode::RtpOutputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RtpOutputNode")
{
}

MediaNodeKind RtpOutputNode::staticKind() noexcept
{
    return MediaNodeKind::RtpOutput;
}

::media::Status RtpOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
