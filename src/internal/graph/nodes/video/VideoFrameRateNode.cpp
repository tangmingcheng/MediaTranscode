#include "internal/graph/nodes/video/VideoFrameRateNode.h"

namespace media::ffmpeg::graph {

VideoFrameRateNode::VideoFrameRateNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "VideoFrameRateNode")
{
}

MediaNodeKind VideoFrameRateNode::staticKind() noexcept
{
    return MediaNodeKind::VideoFrameRate;
}

::media::Status VideoFrameRateNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
