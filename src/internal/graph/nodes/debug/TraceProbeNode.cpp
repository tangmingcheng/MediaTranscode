#include "internal/graph/nodes/debug/TraceProbeNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

TraceProbeNode::TraceProbeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "TraceProbeNode")
{
}

MediaNodeKind TraceProbeNode::staticKind() noexcept
{
    return MediaNodeKind::TraceProbe;
}

::media::Result<MediaNodeProcessResult> TraceProbeNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    auto status = pushToAllOutputs(context, *input.value());
    return (*input.value())->isEof() ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
