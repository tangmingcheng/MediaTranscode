#include "internal/graph/nodes/audio/AudioResampleNode.h"

namespace media::ffmpeg::graph {

AudioResampleNode::AudioResampleNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioResampleNode")
{
}

MediaNodeKind AudioResampleNode::staticKind() noexcept
{
    return MediaNodeKind::AudioResample;
}

::media::Status AudioResampleNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
