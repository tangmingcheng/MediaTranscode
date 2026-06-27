#include "internal/graph/nodes/audio/AudioCopyNode.h"

namespace media::ffmpeg::graph {

AudioCopyNode::AudioCopyNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioCopyNode")
{
}

MediaNodeKind AudioCopyNode::staticKind() noexcept
{
    return MediaNodeKind::AudioCopy;
}

::media::Status AudioCopyNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
