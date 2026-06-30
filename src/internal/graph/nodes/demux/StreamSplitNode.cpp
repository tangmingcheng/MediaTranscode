#include "internal/graph/nodes/demux/StreamSplitNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

StreamSplitNode::StreamSplitNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "StreamSplitNode")
{
}

MediaNodeKind StreamSplitNode::staticKind() noexcept
{
    return MediaNodeKind::StreamSplit;
}

::media::Status StreamSplitNode::onProcess(MediaGraphExecutionContext& context)
{
    auto buffer = tryPopFirstInput(context);
    if (!buffer) {
        return ::media::Status::success();
    }

    const AVPacket* packet = FFmpegPacketView::packet(buffer.value());
    if (!packet) {
        return pushToAllOutputs(context, buffer.value());
    }

    return pushToMatchingOutputs(context,
                                 buffer.value(),
                                 buffer.value()->streamKind(),
                                 packet->stream_index);
}

} // namespace media::ffmpeg::graph
