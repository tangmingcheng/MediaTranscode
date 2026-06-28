#include "internal/graph/nodes/merge/PacketMergeNode.h"

namespace media::ffmpeg::graph {

PacketMergeNode::PacketMergeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketMergeNode")
{
}

MediaNodeKind PacketMergeNode::staticKind() noexcept
{
    return MediaNodeKind::PacketMerge;
}

::media::Status PacketMergeNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
