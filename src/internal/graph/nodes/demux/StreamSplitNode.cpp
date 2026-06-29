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

    bool pushed = false;
    for (MediaChannel* channel : outputChannels(context)) {
        if (!channel) {
            continue;
        }

        const auto& binding = channel->binding();
        const auto& format = channel->formatDescriptor();
        const bool streamKindMatches =
            binding.streamKind == MediaStreamKind::Any ||
            binding.streamKind == MediaStreamKind::Unknown ||
            binding.streamKind == buffer.value()->streamKind();
        const bool streamIndexMatches =
            !format.hasStreamIndex() ||
            format.streamIndex == packet->stream_index;

        if (!streamKindMatches || !streamIndexMatches) {
            continue;
        }

        auto status = channel->push(buffer.value());
        if (!status) {
            return status;
        }
        pushed = true;
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
