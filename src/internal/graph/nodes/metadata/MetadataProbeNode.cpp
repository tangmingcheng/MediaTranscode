#include "internal/graph/nodes/metadata/MetadataProbeNode.h"

namespace media::ffmpeg::graph {

MetadataProbeNode::MetadataProbeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MetadataProbeNode")
{
}

MediaNodeKind MetadataProbeNode::staticKind() noexcept
{
    return MediaNodeKind::MetadataProbe;
}

::media::Status MetadataProbeNode::onProcess(MediaGraphExecutionContext& context)
{
    return forwardFirstInputToAllOutputs(context);
}

} // namespace media::ffmpeg::graph
