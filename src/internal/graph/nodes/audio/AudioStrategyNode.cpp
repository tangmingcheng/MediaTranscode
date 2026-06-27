#include "internal/graph/nodes/audio/AudioStrategyNode.h"

namespace media::ffmpeg::graph {

AudioStrategyNode::AudioStrategyNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioStrategyNode")
{
}

MediaNodeKind AudioStrategyNode::staticKind() noexcept
{
    return MediaNodeKind::AudioStrategy;
}

::media::Status AudioStrategyNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
