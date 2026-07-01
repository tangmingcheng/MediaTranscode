#include "internal/graph/nodes/debug/DebugDumpNode.h"

namespace media::ffmpeg::graph {

DebugDumpNode::DebugDumpNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "DebugDumpNode")
{
}

MediaNodeKind DebugDumpNode::staticKind() noexcept
{
    return MediaNodeKind::DebugDump;
}

::media::Status DebugDumpNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    if (outputChannels(context).empty()) {
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, *input.value());
}

} // namespace media::ffmpeg::graph
