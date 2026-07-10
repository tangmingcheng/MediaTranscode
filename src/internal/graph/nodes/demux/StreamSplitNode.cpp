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

::media::Result<MediaNodeProcessResult> StreamSplitNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return processWaiting();
    }

    const MediaBufferRef& buffer = *input.value();
    const AVPacket* packet = FFmpegPacketView::packet(buffer);
    if (!packet) {
        auto status = broadcastControlToAllOutputs(context, buffer);
        return buffer->isEof() ? processFinished(status) : processProgress(status);
    }

    return processProgress(pushToMatchingOutputs(context,
                                 buffer,
                                 buffer->streamKind(),
                                 packet->stream_index,
                                 RouteMatchPolicy::AllowDrop));
}

} // namespace media::ffmpeg::graph
