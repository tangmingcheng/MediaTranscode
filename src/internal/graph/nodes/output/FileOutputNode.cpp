#include "internal/graph/nodes/output/FileOutputNode.h"

namespace media::ffmpeg::graph {

FileOutputNode::FileOutputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileOutputNode")
{
}

MediaNodeKind FileOutputNode::staticKind() noexcept
{
    return MediaNodeKind::FileOutput;
}

::media::Status FileOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
