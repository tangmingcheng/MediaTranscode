#include "internal/graph/nodes/split/PacketFanoutNode.h"

namespace media::ffmpeg::graph {

PacketFanoutNode::PacketFanoutNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketFanoutNode")
{
}

MediaNodeKind PacketFanoutNode::staticKind() noexcept
{
    return MediaNodeKind::PacketFanout;
}

::media::Status PacketFanoutNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
