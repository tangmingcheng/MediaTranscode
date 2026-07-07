#include "internal/graph/nodes/mux/RtpMuxNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/mathematics.h>
}

#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

AVRational toAVRational(MediaRational value) noexcept
{
    return AVRational{ value.num, value.den };
}

bool known(AVRational value) noexcept
{
    return value.num != 0 && value.den != 0;
}

AVRational packetTimeBase(const MediaBufferRef& buffer) noexcept
{
    if (!buffer) {
        return AVRational{ 0, 1 };
    }
    if (buffer->timeDescriptor().timeBase.isKnown()) {
        return toAVRational(buffer->timeDescriptor().timeBase);
    }
    if (buffer->formatDescriptor().time.timeBase.isKnown()) {
        return toAVRational(buffer->formatDescriptor().time.timeBase);
    }
    return AVRational{ 0, 1 };
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
    auto configured = configureExpectations(context);
    if (!configured) {
        return configured;
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = *input.value();
    if (tryBindOutputContext(buffer)) {
        auto status = registerPendingStreamConfigs();
        if (!status) {
            return status;
        }
        status = writePendingPacketsIfReady();
        return status ? emitFormatIfReady(context) : status;
    }

    auto configStatus = tryBindStreamConfig(buffer);
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
        auto status = writePacket(buffer);
        if (!status) {
            return status;
        }
        return emitFormatIfReady(context);
    }

    return ::media::Status::success();
}

::media::Status RtpMuxNode::stop(MediaGraphExecutionContext& context)
{
    auto pending = writePendingPacketsIfReady();
    if (!pending) {
        releaseRuntimeViews();
        return pending;
    }
    auto trailer = writeTrailerIfNeeded();
    if (!trailer) {
        releaseRuntimeViews();
        return trailer;
    }
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            "rtp_mux.stop packets_written=" + std::to_string(m_packetsWritten));
    auto stopped = FFmpegNodeRuntime::stop(context);
    releaseRuntimeViews();
    return stopped;
}

::media::Status RtpMuxNode::configureExpectations(MediaGraphExecutionContext& context)
{
    if (m_expectationsBound) {
        return ::media::Status::success();
    }
    auto video = requiredBoolNodeOption(nodeOptions(context), "RtpMuxNode", MediaTranscodeOptionKey::MuxExpectVideo);
    if (!video) {
        return ::media::Status::failure(video.error());
    }
    auto audio = requiredBoolNodeOption(nodeOptions(context), "RtpMuxNode", MediaTranscodeOptionKey::MuxExpectAudio);
    if (!audio) {
        return ::media::Status::failure(audio.error());
    }
    if (audio.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::unsupported("RtpMuxNode is video-only in the current realtime DAG"));
    }
    m_expectVideo = video.value();
    m_expectationsBound = true;
    return ::media::Status::success();
}

bool RtpMuxNode::tryBindOutputContext(const MediaBufferRef& buffer) noexcept
{
    auto* contextBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!contextBuffer || !contextBuffer->context()) {
        return false;
    }
    if (contextBuffer->ownership() == FFmpegFormatContextOwnership::Output) {
        m_outputContextOwner = contextBuffer->takeOutputContext();
        m_outputContext = m_outputContextOwner.get();
    } else {
        m_outputContextOwner.reset();
        m_outputContext = contextBuffer->context();
    }
    m_headerWritten = false;
    m_trailerWritten = false;
    m_formatEmitted = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    return m_outputContext != nullptr;
}

::media::Status RtpMuxNode::tryBindStreamConfig(const MediaBufferRef& buffer)
{
    if (!dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get())) {
        return ::media::Status::success();
    }
    if (m_headerWritten) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode received late stream config"));
    }
    if (!m_outputContext) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    auto registered = registerStreamFromCodecContext(buffer);
    return registered ? writePendingPacketsIfReady() : registered;
}

::media::Status RtpMuxNode::registerPendingStreamConfigs()
{
    if (!m_outputContext || m_pendingStreamConfigs.empty()) {
        return ::media::Status::success();
    }
    auto pending = std::move(m_pendingStreamConfigs);
    m_pendingStreamConfigs.clear();
    for (const auto& buffer : pending) {
        auto status = registerStreamFromCodecContext(buffer);
        if (!status) {
            return status;
        }
    }
    return ::media::Status::success();
}

::media::Status RtpMuxNode::registerStreamFromCodecContext(const MediaBufferRef& buffer)
{
    if (m_videoStreamIndex != invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode received duplicate video stream config"));
    }

    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codec = codecBuffer ? codecBuffer->context() : nullptr;
    if (!m_outputContext || !codec || codec->codec_type != AVMEDIA_TYPE_VIDEO || !known(codec->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires video encoder context and time_base"));
    }

    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream(rtp)"));
    }

    const int ret = avcodec_parameters_from_context(stream->codecpar, codec);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avcodec_parameters_from_context(rtp)");
    }
    stream->codecpar->codec_tag = 0;
    stream->time_base = codec->time_base;
    stream->avg_frame_rate = codec->framerate;
    stream->r_frame_rate = codec->framerate;
    m_videoStreamIndex = stream->index;
    return ::media::Status::success();
}

::media::Status RtpMuxNode::writeHeaderIfNeeded()
{
    if (!m_outputContext || m_headerWritten) {
        return ::media::Status::success();
    }
    if (m_outputContext->nb_streams == 0 || !expectedStreamsRegistered()) {
        return ::media::Status::success();
    }
    const int ret = avformat_write_header(m_outputContext, nullptr);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avformat_write_header(rtp)");
    }
    m_headerWritten = true;
    return ::media::Status::success();
}

::media::Status RtpMuxNode::writePendingPacketsIfReady()
{
    auto header = writeHeaderIfNeeded();
    if (!header || !m_headerWritten || m_pendingPackets.empty()) {
        return header;
    }
    auto pending = std::move(m_pendingPackets);
    m_pendingPackets.clear();
    for (const auto& buffer : pending) {
        auto status = writePacketNow(buffer);
        if (!status) {
            return status;
        }
    }
    return ::media::Status::success();
}

::media::Status RtpMuxNode::writePacket(const MediaBufferRef& buffer)
{
    auto header = writeHeaderIfNeeded();
    if (!header) {
        return header;
    }
    if (!m_headerWritten) {
        m_pendingPackets.push_back(buffer);
        return ::media::Status::success();
    }
    auto pending = writePendingPacketsIfReady();
    return pending ? writePacketNow(buffer) : pending;
}

::media::Status RtpMuxNode::writePacketNow(const MediaBufferRef& buffer)
{
    const AVPacket* source = FFmpegPacketView::packet(buffer);
    if (!m_outputContext || !source || m_videoStreamIndex == invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires output context, video stream, and packet"));
    }
    if (m_videoStreamIndex >= static_cast<int>(m_outputContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode packet stream is not registered"));
    }

    AVStream* muxStream = m_outputContext->streams[m_videoStreamIndex];
    const AVRational srcTb = packetTimeBase(buffer);
    const AVRational muxTb = muxStream ? muxStream->time_base : AVRational{ 0, 1 };
    if (!known(srcTb) || !known(muxTb)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires packet time_base"));
    }

    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("av_packet_alloc(rtp)"));
    }
    const int refRet = av_packet_ref(packet.get(), source);
    if (refRet < 0) {
        return FFmpegGraphError::statusFromCode(refRet, "av_packet_ref(rtp)");
    }
    av_packet_rescale_ts(packet.get(), srcTb, muxTb);
    packet->stream_index = m_videoStreamIndex;

    const int writeRet = av_interleaved_write_frame(m_outputContext, packet.get());
    if (writeRet < 0) {
        return FFmpegGraphError::statusFromCode(writeRet, "av_interleaved_write_frame(rtp)");
    }
    ++m_packetsWritten;
    if (m_packetsWritten == 1) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                "rtp_mux.first_packet_written stream=video");
    }
    return ::media::Status::success();
}

::media::Status RtpMuxNode::emitFormatIfReady(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted || outputChannels(context).empty()) {
        return ::media::Status::success();
    }
    auto header = writeHeaderIfNeeded();
    if (!header) {
        return header;
    }
    if (!m_headerWritten) {
        return ::media::Status::success();
    }

    auto buffer = FFmpegBufferFactory::borrowFormatContext(m_outputContext);
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

::media::Status RtpMuxNode::writeTrailerIfNeeded()
{
    if (!m_outputContext || !m_headerWritten || m_trailerWritten) {
        return ::media::Status::success();
    }
    const int ret = av_write_trailer(m_outputContext);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_write_trailer(rtp)");
    }
    m_trailerWritten = true;
    return ::media::Status::success();
}

void RtpMuxNode::releaseRuntimeViews() noexcept
{
    m_pendingStreamConfigs.clear();
    m_pendingPackets.clear();
    m_outputContext = nullptr;
    m_outputContextOwner.reset();
    m_headerWritten = false;
    m_trailerWritten = false;
    m_formatEmitted = false;
    m_expectationsBound = false;
    m_expectVideo = false;
    m_videoStreamIndex = invalidMediaStreamIndex;
    m_packetsWritten = 0;
}

bool RtpMuxNode::expectedStreamsRegistered() const noexcept
{
    return !m_expectVideo || m_videoStreamIndex != invalidMediaStreamIndex;
}

} // namespace media::ffmpeg::graph
