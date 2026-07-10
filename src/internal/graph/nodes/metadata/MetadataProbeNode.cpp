#include "internal/graph/nodes/metadata/MetadataProbeNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

MetadataProbeNode::MetadataProbeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MetadataProbeNode")
{
}

MediaNodeKind MetadataProbeNode::staticKind() noexcept
{
    return MediaNodeKind::MetadataProbe;
}

::media::Result<MediaNodeProcessResult> MetadataProbeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    auto status = pushToAllOutputs(context, *input.value());
    return (*input.value())->isEof() ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
