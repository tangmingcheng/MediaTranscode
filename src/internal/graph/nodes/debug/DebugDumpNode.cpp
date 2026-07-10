#include "internal/graph/nodes/debug/DebugDumpNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {

DebugDumpNode::DebugDumpNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "DebugDumpNode")
{
}

MediaNodeKind DebugDumpNode::staticKind() noexcept
{
    return MediaNodeKind::DebugDump;
}

::media::Result<MediaNodeProcessResult> DebugDumpNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return processWaiting();
    }

    if (outputChannels(context).empty()) {
        return (*input.value())->isEof() ? processFinished() : processProgress();
    }

    auto status = pushToAllOutputs(context, *input.value());
    return (*input.value())->isEof() ? processFinished(status) : processProgress(status);
}

} // namespace media::ffmpeg::graph
