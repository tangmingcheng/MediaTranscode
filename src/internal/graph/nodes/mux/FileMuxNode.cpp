#include "internal/graph/nodes/mux/FileMuxNode.h"

#include "internal/FFmpegRAII.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {
namespace {

int registeredStreamIndexFor(MediaStreamKind streamKind,
                             int videoStreamIndex,
                             int audioStreamIndex) noexcept
{
    switch (streamKind) {
    case MediaStreamKind::Video:
        return videoStreamIndex;
    case MediaStreamKind::Audio:
        return audioStreamIndex;
    default:
        return invalidMediaStreamIndex;
    }
}

} // namespace

FileMuxNode::FileMuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
{
}

MediaNodeKind FileMuxNode::staticKind() noexcept
{
    return MediaNodeKind::FileMux;
}

::media::Status FileMuxNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = input.value();

    if (tryBindOutputContext(buffer)) {
        return registerPendingCodecContexts();
    }

    auto codecStatus = tryBindCodecContext(buffer);
    if (!codecStatus) {
        return codecStatus;
    }

    if (buffer->isEof() || buffer->isFlush()) {
        return forwardIfOutputsExist(context, buffer);
    }

    if (FFmpegPacketView::isPacket(buffer)) {
        auto writeStatus = writePacket(buffer);
        if (!writeStatus) {
            return writeStatus;
        }
    }

    return forwardIfOutputsExist(context, buffer);
}

::media::Status FileMuxNode::flush(MediaGraphExecutionContext& context)
{
    auto status = writeTrailerIfNeeded();
    if (!status) {
        return status;
    }

    return FFmpegNodeRuntime::flush(context);
}

::media::Status FileMuxNode::stop(MediaGraphExecutionContext& context)
{
    auto status = writeTrailerIfNeeded();
    if (!status) {
        return status;
    }

    return FFmpegNodeRuntime::stop(context);
}

bool FileMuxNode::tryBindOutputContext(const MediaBufferRef& buffer) noexcept
{
    auto* contextBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!contextBuffer || !contextBuffer->context()) {
        return false;
    }

    m_outputContextOwner = buffer;
    m_outputContext = contextBuffer->context();
    m_headerWritten = false;
    m_trailerWritten = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    m_audioStreamIndex = invalidMediaStreamIndex;
    return true;
}

::media::Status FileMuxNode::tryBindCodecContext(const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    if (!codecBuffer || !codecBuffer->context()) {
        return ::media::Status::success();
    }

    if (m_headerWritten) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode received codec context after header was written"));
    }

    if (!m_outputContext) {
        m_pendingCodecContexts.push_back(buffer);
        return ::media::Status::success();
    }

    return registerStreamFromCodecContext(buffer);
}

::media::Status FileMuxNode::registerPendingCodecContexts()
{
    if (!m_outputContext || m_pendingCodecContexts.empty()) {
        return ::media::Status::success();
    }

    auto pending = std::move(m_pendingCodecContexts);
    m_pendingCodecContexts.clear();
    for (const auto& buffer : pending) {
        auto status = registerStreamFromCodecContext(buffer);
        if (!status) {
            return status;
        }
    }

    return ::media::Status::success();
}

::media::Status FileMuxNode::registerStreamFromCodecContext(const MediaBufferRef& buffer)
{
    if (!m_outputContext) {
        m_pendingCodecContexts.push_back(buffer);
        return ::media::Status::success();
    }

    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codecContext = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codecContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode expected FFmpegCodecContextBuffer"));
    }

    const MediaStreamKind streamKind = buffer->streamKind();
    if (registeredStreamIndexFor(streamKind, m_videoStreamIndex, m_audioStreamIndex) != invalidMediaStreamIndex) {
        return ::media::Status::success();
    }

    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("FileMuxNode failed: avformat_new_stream returned null"));
    }

    const int copyRet = avcodec_parameters_from_context(stream->codecpar, codecContext);
    if (copyRet < 0) {
        return FFmpegGraphError::statusFromCode(copyRet, "avcodec_parameters_from_context");
    }

    stream->time_base = codecContext->time_base;
    if (streamKind == MediaStreamKind::Video) {
        stream->avg_frame_rate = codecContext->framerate;
        stream->r_frame_rate = codecContext->framerate;
        m_videoStreamIndex = stream->index;
    } else if (streamKind == MediaStreamKind::Audio) {
        m_audioStreamIndex = stream->index;
    }

    return ::media::Status::success();
}

::media::Status FileMuxNode::writeHeaderIfNeeded()
{
    if (!m_outputContext || m_headerWritten) {
        return ::media::Status::success();
    }

    if (m_outputContext->nb_streams == 0) {
        return ::media::Status::success();
    }

    const int ret = avformat_write_header(m_outputContext, nullptr);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avformat_write_header");
    }

    m_headerWritten = true;
    return ::media::Status::success();
}

::media::Status FileMuxNode::writePacket(const MediaBufferRef& buffer)
{
    if (!m_outputContext) {
        return ::media::Status::success();
    }

    const AVPacket* sourcePacket = FFmpegPacketView::packet(buffer);
    if (!sourcePacket) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode expected packet buffer"));
    }

    const int targetStreamIndex = registeredStreamIndexFor(buffer->streamKind(),
                                                           m_videoStreamIndex,
                                                           m_audioStreamIndex);
    if (targetStreamIndex == invalidMediaStreamIndex) {
        return ::media::Status::success();
    }

    auto headerStatus = writeHeaderIfNeeded();
    if (!headerStatus) {
        return headerStatus;
    }

    if (!m_headerWritten) {
        return ::media::Status::success();
    }

    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("FileMuxNode failed: av_packet_alloc returned null"));
    }

    const int refRet = av_packet_ref(packet.get(), sourcePacket);
    if (refRet < 0) {
        return FFmpegGraphError::statusFromCode(refRet, "av_packet_ref(mux packet)");
    }

    packet->stream_index = targetStreamIndex;

    const int ret = av_interleaved_write_frame(m_outputContext, packet.get());
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_interleaved_write_frame");
    }

    return ::media::Status::success();
}

::media::Status FileMuxNode::writeTrailerIfNeeded()
{
    if (!m_outputContext || !m_headerWritten || m_trailerWritten) {
        return ::media::Status::success();
    }

    const int ret = av_write_trailer(m_outputContext);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_write_trailer");
    }

    m_trailerWritten = true;
    return ::media::Status::success();
}

::media::Status FileMuxNode::forwardIfOutputsExist(MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    if (outputChannels(context).empty()) {
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, buffer);
}

} // namespace media::ffmpeg::graph
