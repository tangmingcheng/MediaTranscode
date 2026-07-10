#include "internal/graph/nodes/control/ControlSignalNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

ControlSignalNode::ControlSignalNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "ControlSignalNode")
{
}

MediaNodeKind ControlSignalNode::staticKind() noexcept
{
    return MediaNodeKind::ControlSignal;
}

::media::Result<MediaNodeProcessResult> ControlSignalNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    auto status = pushToAllOutputs(context, *input.value());
    return (*input.value())->isEof() ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
