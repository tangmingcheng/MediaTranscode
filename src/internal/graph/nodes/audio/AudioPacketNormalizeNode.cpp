#include "internal/graph/nodes/audio/AudioPacketNormalizeNode.h"

namespace media::ffmpeg::graph {

AudioPacketNormalizeNode::AudioPacketNormalizeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioPacketNormalizeNode")
{
}

MediaNodeKind AudioPacketNormalizeNode::staticKind() noexcept
{
    return MediaNodeKind::AudioPacketNormalize;
}

::media::Status AudioPacketNormalizeNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
