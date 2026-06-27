#include "internal/graph/nodes/video/VideoPacketDrainNode.h"

namespace media::ffmpeg::graph {

VideoPacketDrainNode::VideoPacketDrainNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoPacketDrainNode")
{
}

MediaNodeKind VideoPacketDrainNode::staticKind() noexcept
{
    return MediaNodeKind::VideoPacketDrain;
}

::media::Status VideoPacketDrainNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
