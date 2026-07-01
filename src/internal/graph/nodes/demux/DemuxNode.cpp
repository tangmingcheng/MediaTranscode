#include "internal/graph/nodes/demux/DemuxNode.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegPacketBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <utility>

namespace media::ffmpeg::graph {

DemuxNode::DemuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "DemuxNode")
{
}

MediaNodeKind DemuxNode::staticKind() noexcept
{
    return MediaNodeKind::Demux;
}

::media::Status DemuxNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_formatContext) {
        auto status = bindFormatContext(context);
        if (!status) {
            return status;
        }

        if (!m_formatContext) {
            return ::media::Status::success();
        }
    }

    if (m_eofSent) {
        return ::media::Status::success();
    }

    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("DemuxNode failed: av_packet_alloc returned null"));
    }

    const int ret = av_read_frame(m_formatContext, packet.get());
    if (ret == AVERROR_EOF) {
        return emitEof(context);
    }

    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_read_frame");
    }

    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    if (packet->stream_index >= 0 && packet->stream_index < static_cast<int>(m_formatContext->nb_streams)) {
        streamKind = FFmpegDescriptorMapper::toStreamKind(
            m_formatContext->streams[packet->stream_index]->codecpar->codec_type);
    }

    auto buffer = FFmpegBufferFactory::wrapPacket(std::move(packet), streamKind);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.value().get());
    const int streamIndex = packetBuffer && packetBuffer->packet()
                                ? packetBuffer->packet()->stream_index
                                : invalidMediaStreamIndex;

    return pushToMatchingOutputs(context, buffer.value(), streamKind, streamIndex);
}

::media::Status DemuxNode::bindFormatContext(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.value()->get());
    if (!formatBuffer || !formatBuffer->context()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("DemuxNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(*input.value());
    m_formatContext = formatBuffer->context();
    return ::media::Status::success();
}

::media::Status DemuxNode::emitEof(MediaGraphExecutionContext& context)
{
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    if (!eof) {
        return ::media::Status::failure(eof.error());
    }

    m_eofSent = true;
    return pushToAllOutputs(context, eof.value());
}

} // namespace media::ffmpeg::graph
