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
    auto buffer = tryPopFirstInput(context);
    if (!buffer) {
        return ::media::Status::success();
    }

    if (outputChannels(context).empty()) {
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, buffer.value());
}

} // namespace media::ffmpeg::graph
