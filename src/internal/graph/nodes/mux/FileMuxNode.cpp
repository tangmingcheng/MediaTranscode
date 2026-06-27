#include "internal/graph/nodes/mux/FileMuxNode.h"

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

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
        return ::media::Status::success();
    }

    if (buffer->isEof() || buffer->isFlush()) {
        auto trailerStatus = writeTrailerIfNeeded();
        if (!trailerStatus) {
            return trailerStatus;
        }
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
    return true;
}

::media::Status FileMuxNode::writeHeaderIfNeeded()
{
    if (!m_outputContext || m_headerWritten) {
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

    auto headerStatus = writeHeaderIfNeeded();
    if (!headerStatus) {
        return headerStatus;
    }

    AVPacket* packet = FFmpegPacketView::writablePacket(buffer);
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileMuxNode expected packet buffer"));
    }

    const int ret = av_interleaved_write_frame(m_outputContext, packet);
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
