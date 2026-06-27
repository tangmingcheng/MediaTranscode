#include "internal/graph/nodes/video/VideoTimestampNode.h"

namespace media::ffmpeg::graph {

VideoTimestampNode::VideoTimestampNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoTimestampNode")
{
}

MediaNodeKind VideoTimestampNode::staticKind() noexcept
{
    return MediaNodeKind::VideoTimestamp;
}

::media::Status VideoTimestampNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
