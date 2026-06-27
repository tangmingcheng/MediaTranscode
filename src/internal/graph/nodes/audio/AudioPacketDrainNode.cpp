#include "internal/graph/nodes/audio/AudioPacketDrainNode.h"

namespace media::ffmpeg::graph {

AudioPacketDrainNode::AudioPacketDrainNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioPacketDrainNode")
{
}

MediaNodeKind AudioPacketDrainNode::staticKind() noexcept
{
    return MediaNodeKind::AudioPacketDrain;
}

::media::Status AudioPacketDrainNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
