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
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    const MediaBufferRef& buffer = *input.value();
    const AVPacket* packet = FFmpegPacketView::packet(buffer);
    if (!packet) {
        return pushToAllOutputs(context, buffer);
    }

    return pushToMatchingOutputs(context,
                                 buffer,
                                 buffer->streamKind(),
                                 packet->stream_index);
}

} // namespace media::ffmpeg::graph
