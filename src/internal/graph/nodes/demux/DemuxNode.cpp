#include "internal/graph/nodes/demux/DemuxNode.h"
#include "internal/graph/protocol/FFmpegInputReadTermination.h"

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaDemuxInputBuffer.h"
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

namespace {

int demuxInterruptCallback(void* opaque)
{
    auto* node = static_cast<DemuxNode*>(opaque);
    return node && node->abortRequested() ? 1 : 0;
}

} // namespace

DemuxNode::DemuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "DemuxNode")
{
}

MediaNodeKind DemuxNode::staticKind() noexcept
{
    return MediaNodeKind::Demux;
}

::media::Status DemuxNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    return FFmpegNodeRuntime::start(context);
}

::media::Result<MediaNodeProcessResult> DemuxNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_formatContext) {
        auto status = bindFormatContext(context);
        if (!status) {
            return processProgress(status);
        }

        if (!m_formatContext) {
            return processWaiting();
        }
    }

    if (m_eofSent) {
        return processFinished();
    }

    ::media::ffmpeg::PacketPtr packet;
    MediaDemuxPacketProvenance provenance{
        MediaDemuxPacketOrigin::LiveDemuxRead,
        m_session->nextLiveOrdinal};
    if (!m_session->replay.empty()) {
        MediaDemuxPreparedPacket replay = std::move(m_session->replay.front());
        m_session->replay.pop_front();
        packet = std::move(replay.packet);
        provenance = replay.provenance;
    } else {
        packet = ::media::ffmpeg::makePacket();
        if (!packet) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::allocationFailed("DemuxNode failed: av_packet_alloc returned null"));
        }
        const int ret = av_read_frame(m_formatContext, packet.get());
        if (ret < 0) {
        const auto termination = classifyFFmpegInputReadTermination(
            ret, m_abortRequested.load(), "DemuxNode av_read_frame");
        if (termination.kind() == FFmpegInputReadTerminationKind::EndOfStream) {
            return processFinished(emitEof(context));
        }
            return processProgress(
                ::media::Status::failure(*termination.error()));
        }
        provenance.ordinal = m_session->nextLiveOrdinal++;
    }

    MediaStreamKind streamKind = MediaStreamKind::Unknown;
    if (packet->stream_index >= 0 && packet->stream_index < static_cast<int>(m_formatContext->nb_streams)) {
        streamKind = FFmpegDescriptorMapper::toStreamKind(
            m_formatContext->streams[packet->stream_index]->codecpar->codec_type);
    }

    auto buffer = FFmpegBufferFactory::wrapPacket(
        std::move(packet), streamKind, std::nullopt, provenance);
    if (!buffer) {
        return ::media::Result<MediaNodeProcessResult>::failure(buffer.error());
    }

    const auto* packetBuffer = dynamic_cast<const FFmpegPacketBuffer*>(buffer.value().get());
    const int streamIndex = packetBuffer && packetBuffer->packet()
                                ? packetBuffer->packet()->stream_index
                                : invalidMediaStreamIndex;

    return processProgress(pushToMatchingOutputs(context, buffer.value(), streamKind, streamIndex));
}

::media::Status DemuxNode::stop(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    return FFmpegNodeRuntime::stop(context);
}

void DemuxNode::abort(MediaGraphExecutionContext& context) noexcept
{
    resetRuntimeState();
    FFmpegNodeRuntime::abort(context);
}

void DemuxNode::interrupt(MediaGraphExecutionContext&) noexcept
{
    m_abortRequested = true;
}

bool DemuxNode::abortRequested() const noexcept
{
    return m_abortRequested.load();
}

bool DemuxNode::hasBoundFormatContext() const noexcept
{
    return m_formatContext != nullptr && m_session && m_session->context;
}

void DemuxNode::resetRuntimeState() noexcept
{
    if (m_formatContext) {
        m_formatContext->interrupt_callback.callback = nullptr;
        m_formatContext->interrupt_callback.opaque = nullptr;
    }
    m_formatContext = nullptr;
    m_session.reset();
    m_eofSent = false;
    m_abortRequested = false;
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

    auto* demuxInput = dynamic_cast<MediaDemuxInputBuffer*>(input.value()->get());
    if (!demuxInput) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("DemuxNode expected MediaDemuxInputBuffer"));
    }
    auto session = demuxInput->takeDemuxSession();
    if (!session) return ::media::Status::failure(session.error());
    m_session.emplace(std::move(session).value());
    m_formatContext = m_session->context.get();
    if (!m_formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("DemuxNode requires exclusive input format context ownership"));
    }
    m_abortRequested = false;
    m_formatContext->interrupt_callback.callback = demuxInterruptCallback;
    m_formatContext->interrupt_callback.opaque = this;
    return ::media::Status::success();
}

::media::Status DemuxNode::emitEof(MediaGraphExecutionContext& context)
{
    auto eof = FFmpegBufferFactory::makeEof(MediaStreamKind::Control);
    if (!eof) {
        return ::media::Status::failure(eof.error());
    }

    m_eofSent = true;
    return broadcastControlToAllOutputs(context, eof.value());
}

} // namespace media::ffmpeg::graph
