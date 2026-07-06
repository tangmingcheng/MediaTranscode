#include "internal/graph/nodes/mux/RtpMuxNode.h"

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

bool boolOption(const MediaNodeOptions* options, const char* key, bool fallback)
{
    if (!options || !options->has(key)) {
        return fallback;
    }
    const std::string value = options->value(key);
    if (value == "1" || value == "true" || value == "yes" || value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off") {
        return false;
    }
    return fallback;
}

} // namespace

RtpMuxNode::RtpMuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RtpMuxNode")
{
}

MediaNodeKind RtpMuxNode::staticKind() noexcept
{
    return MediaNodeKind::RtpMux;
}

::media::Status RtpMuxNode::onProcess(MediaGraphExecutionContext& context)
{
    auto expected = configureExpectations(context);
    if (!expected) {
        return expected;
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = *input.value();
    if (m_writer.bindOutputContext(buffer)) {
        auto status = m_writer.registerPendingStreamConfigs();
        if (!status) {
            return status;
        }
        status = m_writer.writePendingPacketsIfReady();
        return status ? emitFormatIfReady(context) : status;
    }

    auto configStatus = m_writer.tryBindStreamConfig(buffer);
    if (!configStatus) {
        return configStatus;
    }
    auto sdpStatus = emitFormatIfReady(context);
    if (!sdpStatus) {
        return sdpStatus;
    }

    if (buffer->isEof() || buffer->isFlush()) {
        return ::media::Status::success();
    }

    if (FFmpegPacketView::isPacket(buffer)) {
        auto status = m_writer.writePacket(buffer);
        if (!status) {
            return status;
        }
        return emitFormatIfReady(context);
    }

    return ::media::Status::success();
}

::media::Status RtpMuxNode::stop(MediaGraphExecutionContext& context)
{
    auto pending = m_writer.writePendingPacketsIfReady();
    if (!pending) {
        releaseRuntimeViews();
        return pending;
    }
    auto trailer = m_writer.writeTrailerIfNeeded();
    if (!trailer) {
        releaseRuntimeViews();
        return trailer;
    }
    auto stopped = FFmpegNodeRuntime::stop(context);
    releaseRuntimeViews();
    return stopped;
}

void RtpMuxNode::abort(MediaGraphExecutionContext& context) noexcept
{
    releaseRuntimeViews();
    FFmpegNodeRuntime::abort(context);
}

::media::Status RtpMuxNode::configureExpectations(MediaGraphExecutionContext& context)
{
    if (m_expectationsBound) {
        return ::media::Status::success();
    }
    const MediaNodeOptions* options = nodeOptions(context);
    const bool expectVideo = boolOption(options, MediaTranscodeOptionKey::MuxExpectVideo, true);
    const bool expectAudio = boolOption(options, MediaTranscodeOptionKey::MuxExpectAudio, false);
    m_expectationsBound = true;
    return m_writer.configureExpectations(expectVideo, expectAudio);
}

::media::Status RtpMuxNode::emitFormatIfReady(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted || outputChannels(context).empty() || !m_writer.readyForSdp()) {
        return ::media::Status::success();
    }

    auto header = m_writer.writeHeaderIfNeeded();
    if (!header) {
        return header;
    }
    if (!m_writer.headerWritten()) {
        return ::media::Status::success();
    }

    auto buffer = FFmpegBufferFactory::borrowFormatContext(m_writer.context());
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    auto pushed = emitOutput(context, "format", buffer.value());
    if (!pushed) {
        return pushed;
    }
    m_formatEmitted = true;
    return ::media::Status::success();
}

void RtpMuxNode::releaseRuntimeViews() noexcept
{
    m_writer.reset();
    m_expectationsBound = false;
    m_formatEmitted = false;
}

} // namespace media::ffmpeg::graph
