#include "internal/graph/nodes/mux/RtpMuxNode.h"
#include "internal/graph/nodes/mux/RtpMuxProtocolIo.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacedAvio.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/mathematics.h>
}

#include <string>
#include <utility>
#include <limits>
#include <algorithm>
#include <chrono>
#include <thread>

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

MediaTimeValue packetPacingTimestamp(const AVPacket* packet) noexcept
{
    if (!packet) {
        return invalidMediaTimeValue;
    }
    if (packet->dts != AV_NOPTS_VALUE) {
        return packet->dts;
    }
    if (packet->pts != AV_NOPTS_VALUE) {
        return packet->pts;
    }
    return invalidMediaTimeValue;
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

::media::Result<MediaNodeProcessResult> RtpMuxNode::onProcess(MediaGraphExecutionContext& context)
{
    auto configured = configureExpectations(context);
    if (!configured) {
        return processProgress(configured);
    }
    if (m_state.completion().readyForTrailer()) {
        auto pending = writePendingPacketsIfReady();
        if (!pending) return processProgress(pending);
        auto trailer = writeTrailerIfNeeded();
        return processFinished(trailer);
    }

    auto input = tryPopFirstInputWithChannelOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        bool allClosed = true;
        for (MediaChannel* channel : context.inputChannels(nodeId())) {
            allClosed = allClosed && channel->closed() && channel->size() == 0;
            if (channel->closed() && channel->size() == 0 &&
                channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
                m_state.completion().markInputClosed(std::to_string(channel->id().value));
            }
        }
        if (allClosed && m_state.completion().readyForTrailer()) {
            auto pending = writePendingPacketsIfReady();
            if (!pending) return processProgress(pending);
            auto trailer = writeTrailerIfNeeded();
            if (!trailer) return processProgress(trailer);
            return processFinished();
        }
        return processWaiting();
    }

    MediaBufferRef buffer = input.value()->buffer;
    if (tryBindOutputContext(buffer)) {
        auto status = registerPendingStreamConfigs();
        if (!status) {
            return processProgress(status);
        }
        status = writePendingPacketsIfReady();
        return processProgress(status ? emitFormatIfReady(context) : status);
    }

    auto configStatus = tryBindStreamConfig(buffer);
    if (!configStatus) {
        return processProgress(configStatus);
    }

    auto sdpStatus = emitFormatIfReady(context);
    if (!sdpStatus) {
        return processProgress(sdpStatus);
    }

    if (buffer->isEof()) {
        m_state.completion().markInputEof(std::to_string(input.value()->channel->id().value));
        if (!m_state.completion().readyForTrailer()) return processProgress();
        auto pending = writePendingPacketsIfReady();
        if (!pending) return processProgress(pending);
        auto trailer = writeTrailerIfNeeded();
        if (!trailer) return processProgress(trailer);
        return processFinished();
    }
    if (buffer->isFlush()) {
        return processProgress();
    }

    if (FFmpegPacketView::isPacket(buffer)) {
        auto status = writePacket(buffer);
        if (!status) {
            return processProgress(status);
        }
        return processProgress(emitFormatIfReady(context));
    }

    return processProgress();
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
                            "rtp_mux.stop packets_written=" + std::to_string(m_session.packetsWritten()));
    auto stopped = FFmpegNodeRuntime::stop(context);
    releaseRuntimeViews();
    return stopped;
}

::media::Status RtpMuxNode::configureExpectations(MediaGraphExecutionContext& context)
{
    if (m_state.expectationsBound()) {
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
    if (video.value() == audio.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires exactly one media stream kind"));
    }
    m_state.expectVideo() = video.value();
    m_state.expectAudio() = audio.value();
    std::vector<std::string> terminalInputs;
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (channel && channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
            terminalInputs.push_back(std::to_string(channel->id().value));
        }
    }
    m_state.completion().setExpectedConfigKeys({expectedStreamName()});
    m_state.completion().setExpectedTerminalChannels(std::move(terminalInputs));

    auto pacing = requiredBoolNodeOption(nodeOptions(context), "RtpMuxNode", "rtp.pacing.enabled");
    if (!pacing) {
        return ::media::Status::failure(pacing.error());
    }
    MediaLatencyPolicy pacingPolicy;
    pacingPolicy.mode = MediaLatencyMode::Realtime;
    pacingPolicy.enablePacing = pacing.value();
    m_session.pacingClock().setPolicy(pacingPolicy);
    m_session.pacingClock().reset();

    auto monotonicPacketTimestamps = requiredBoolNodeOption(nodeOptions(context),
                                                           "RtpMuxNode",
                                                           "rtp.packet_timestamps.monotonic");
    if (!monotonicPacketTimestamps) {
        return ::media::Status::failure(monotonicPacketTimestamps.error());
    }
    m_state.monotonicPacketTimestamps() = monotonicPacketTimestamps.value();

    auto startupDelayMs = requiredNonNegativeIntNodeOption(nodeOptions(context),
                                                          "RtpMuxNode",
                                                          "rtp.startup_delay_ms");
    if (!startupDelayMs) {
        return ::media::Status::failure(startupDelayMs.error());
    }
    m_state.startupDelayMs() = startupDelayMs.value();
    m_state.startupDelayElapsed() = m_state.startupDelayMs() == 0;

    m_state.expectationsBound() = true;
    return ::media::Status::success();
}

bool RtpMuxNode::tryBindOutputContext(const MediaBufferRef& buffer) noexcept
{
    if (!m_session.bindOutput(buffer)) return false;
    m_state.headerWritten() = false;
    m_state.trailerWritten() = false;
    m_state.formatEmitted() = false;
    return true;
}

::media::Status RtpMuxNode::tryBindStreamConfig(const MediaBufferRef& buffer)
{
    const bool accepted = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get()) ||
        dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get());
    if (!accepted) {
        return ::media::Status::success();
    }
    if (m_state.headerWritten()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode received late stream config"));
    }
    if (!m_session.context()) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    auto registered = registerStreamFromConfig(buffer);
    return registered ? writePendingPacketsIfReady() : registered;
}

::media::Status RtpMuxNode::registerPendingStreamConfigs()
{
    if (!m_session.context() || m_pendingStreamConfigs.empty()) {
        return ::media::Status::success();
    }
    auto pending = std::move(m_pendingStreamConfigs);
    m_pendingStreamConfigs.clear();
    for (const auto& buffer : pending) {
        auto status = registerStreamFromConfig(buffer);
        if (!status) {
            return status;
        }
    }
    return ::media::Status::success();
}

::media::Status RtpMuxNode::registerStreamFromConfig(const MediaBufferRef& buffer)
{
    if (!m_session.context()) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    if (dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get())) {
        return registerStreamFromCodecContext(buffer);
    }
    if (dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get())) {
        return registerStreamFromCodecParameters(buffer);
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("RtpMuxNode expected stream config"));
}

::media::Status RtpMuxNode::registerStreamFromCodecContext(const MediaBufferRef& buffer)
{
    if (m_session.streamIndex() != invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode received duplicate stream config"));
    }

    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codec = codecBuffer ? codecBuffer->context() : nullptr;
    const AVMediaType expectedType = m_state.expectAudio() ? AVMEDIA_TYPE_AUDIO : AVMEDIA_TYPE_VIDEO;
    if (!m_session.context() || !codec || codec->codec_type != expectedType || !known(codec->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires matching encoder context and time_base"));
    }

    AVStream* stream = avformat_new_stream(m_session.context(), nullptr);
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
    m_session.streamIndex() = stream->index;
    m_state.completion().markConfigReady(expectedStreamName());
    return ::media::Status::success();
}

::media::Status RtpMuxNode::registerStreamFromCodecParameters(const MediaBufferRef& buffer)
{
    if (m_session.streamIndex() != invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode received duplicate stream config"));
    }

    auto* paramsBuffer = dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get());
    const AVCodecParameters* params = paramsBuffer ? paramsBuffer->parameters() : nullptr;
    const AVMediaType expectedType = m_state.expectAudio() ? AVMEDIA_TYPE_AUDIO : AVMEDIA_TYPE_VIDEO;
    if (!m_session.context() || !params || params->codec_type != expectedType || !buffer->timeDescriptor().timeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires matching codec parameters and time_base"));
    }

    AVStream* stream = avformat_new_stream(m_session.context(), nullptr);
    if (!stream) {
        return ::media::Status::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream(rtp)"));
    }

    const int ret = avcodec_parameters_copy(stream->codecpar, params);
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "avcodec_parameters_copy(rtp)");
    }
    stream->codecpar->codec_tag = 0;
    stream->time_base = toAVRational(buffer->timeDescriptor().timeBase);
    m_session.streamIndex() = stream->index;
    m_state.completion().markConfigReady(expectedStreamName());
    return ::media::Status::success();
}

MediaStreamKind RtpMuxNode::expectedStreamKind() const noexcept
{
    return m_state.expectAudio() ? MediaStreamKind::Audio : MediaStreamKind::Video;
}

const char* RtpMuxNode::expectedStreamName() const noexcept
{
    return m_state.expectAudio() ? "audio" : "video";
}

::media::Status RtpMuxNode::writeHeaderIfNeeded()
{
    if (!m_session.context() || m_state.headerWritten()) {
        return ::media::Status::success();
    }
    if (m_session.context()->nb_streams == 0 || !expectedStreamsRegistered()) {
        return ::media::Status::success();
    }
    auto written = RtpMuxProtocolIo::writeHeader(*m_session.context());
    if (!written) return written;
    m_state.headerWritten() = true;
    m_state.completion().markHeaderWritten();
    return ::media::Status::success();
}

::media::Status RtpMuxNode::writePendingPacketsIfReady()
{
    auto header = writeHeaderIfNeeded();
    if (!header || !m_state.headerWritten() || m_pendingPackets.empty()) {
        return header;
    }
    auto pending = std::move(m_pendingPackets);
    m_pendingPackets.clear();
    m_state.completion().setPendingPackets(0);
    for (const auto& buffer : pending) {
        auto status = writePacketNow(buffer);
        if (!status) {
            return status;
        }
    }
    return ::media::Status::success();
}

::media::Status RtpMuxNode::startPacingSessionIfNeeded()
{
    if (m_state.pacingSessionStarted()) {
        return ::media::Status::success();
    }

    if (!m_state.startupDelayElapsed()) {
        if (m_startupReadyAt.time_since_epoch().count() == 0) {
            m_startupReadyAt = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(m_state.startupDelayMs());
        }
        const auto now = std::chrono::steady_clock::now();
        if (m_startupReadyAt > now) {
            std::this_thread::sleep_until(m_startupReadyAt);
        }
        m_state.startupDelayElapsed() = true;
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                std::string("rtp_mux.startup_delay_elapsed stream=") + expectedStreamName() +
                                    " delay_ms=" + std::to_string(m_state.startupDelayMs()));
    }

    m_session.pacingClock().reset();
    if (m_session.context() && m_session.context()->pb) {
        ::media::ffmpeg::resetPacedWriteAvio(m_session.context()->pb);
    }
    m_state.pacingSessionStarted() = true;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("rtp_mux.pacing_session_started stream=") + expectedStreamName());
    return ::media::Status::success();
}

::media::Status RtpMuxNode::writePacket(const MediaBufferRef& buffer)
{
    auto header = writeHeaderIfNeeded();
    if (!header) {
        return header;
    }
    if (!m_state.headerWritten()) {
        m_pendingPackets.push_back(buffer);
        m_state.completion().setPendingPackets(m_pendingPackets.size());
        return ::media::Status::success();
    }
    auto pending = writePendingPacketsIfReady();
    return pending ? writePacketNow(buffer) : pending;
}

::media::Status RtpMuxNode::writePacketNow(const MediaBufferRef& buffer)
{
    const AVPacket* source = FFmpegPacketView::packet(buffer);
    if (!m_session.context() || !source || m_session.streamIndex() == invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires output context, stream, and packet"));
    }
    if (buffer->streamKind() != expectedStreamKind()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode packet stream kind does not match configured mux"));
    }
    if (m_session.streamIndex() >= static_cast<int>(m_session.context()->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode packet stream is not registered"));
    }

    AVStream* muxStream = m_session.context()->streams[m_session.streamIndex()];
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
    packet->stream_index = m_session.streamIndex();
    if (auto status = normalizePacketTimestamps(*packet); !status) {
        return status;
    }

    const MediaTimeValue pacingTimestamp = packetPacingTimestamp(packet.get());
    if (pacingTimestamp == invalidMediaTimeValue) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires packet dts or pts for RTP pacing"));
    }
    if (auto pacingSession = startPacingSessionIfNeeded(); !pacingSession) {
        return pacingSession;
    }
    auto paced = m_session.pacingClock().waitUntil(pacingTimestamp, MediaRational{ muxTb.num, muxTb.den });
    if (!paced) {
        return paced;
    }

    auto written = RtpMuxProtocolIo::writePacket(*m_session.context(), *packet);
    if (!written) return written;
    ++m_session.packetsWritten();
    if (m_session.packetsWritten() == 1) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                std::string("rtp_mux.first_packet_written stream=") + expectedStreamName());
    }
    return ::media::Status::success();
}

::media::Status RtpMuxNode::normalizePacketTimestamps(AVPacket& packet)
{
    if (!m_state.monotonicPacketTimestamps()) {
        return ::media::Status::success();
    }

    if (packet.dts == AV_NOPTS_VALUE && packet.pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode monotonic RTP timestamps require dts or pts"));
    }

    int64_t shift = 0;
    if (m_session.nextPacketDts() != AV_NOPTS_VALUE) {
        if (packet.dts != AV_NOPTS_VALUE && packet.dts < m_session.nextPacketDts()) {
            shift = std::max(shift, m_session.nextPacketDts() - packet.dts);
        }
        if (packet.pts != AV_NOPTS_VALUE && packet.pts < m_session.nextPacketDts()) {
            shift = std::max(shift, m_session.nextPacketDts() - packet.pts);
        }
    }
    if (shift > 0) {
        if (packet.pts != AV_NOPTS_VALUE) {
            if (packet.pts > std::numeric_limits<int64_t>::max() - shift) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument("RtpMuxNode packet pts overflow"));
            }
            packet.pts += shift;
        }
        if (packet.dts != AV_NOPTS_VALUE) {
            if (packet.dts > std::numeric_limits<int64_t>::max() - shift) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument("RtpMuxNode packet dts overflow"));
            }
            packet.dts += shift;
        }
    }

    int64_t normalizedDts = packet.dts != AV_NOPTS_VALUE ? packet.dts : packet.pts;
    if (packet.pts != AV_NOPTS_VALUE && packet.pts > normalizedDts) {
        normalizedDts = packet.pts;
    }
    const int64_t duration = packet.duration > 0 ? packet.duration : 1;
    if (normalizedDts > std::numeric_limits<int64_t>::max() - duration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode packet timestamp cannot advance past int64 max"));
    }
    m_session.nextPacketDts() = normalizedDts + duration;
    return ::media::Status::success();
}

::media::Status RtpMuxNode::emitFormatIfReady(MediaGraphExecutionContext& context)
{
    if (m_state.formatEmitted() || outputChannels(context).empty()) {
        return ::media::Status::success();
    }
    auto header = writeHeaderIfNeeded();
    if (!header) {
        return header;
    }
    if (!m_state.headerWritten()) {
        return ::media::Status::success();
    }

    auto buffer = makeSdpFormatSnapshot();
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    auto pushed = emitOutput(context, "format", buffer.value());
    if (!pushed) {
        return pushed;
    }
    m_state.formatEmitted() = true;
    return ::media::Status::success();
}

::media::Result<MediaBufferRef> RtpMuxNode::makeSdpFormatSnapshot() const
{
    if (!m_session.context() || m_session.streamIndex() == invalidMediaStreamIndex) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("RtpMuxNode requires output context and stream before SDP snapshot"));
    }

    AVFormatContext* raw = nullptr;
    const char* url = (m_session.context()->url && m_session.context()->url[0] != '\0')
        ? m_session.context()->url
        : nullptr;
    const int allocRet = avformat_alloc_output_context2(&raw, nullptr, "rtp", url);
    if (allocRet < 0 || !raw) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(allocRet < 0 ? allocRet : AVERROR_UNKNOWN,
                                       "avformat_alloc_output_context2(rtp sdp snapshot)"));
    }

    ::media::ffmpeg::OutputFormatContextPtr snapshot(raw);
    snapshot->packet_size = m_session.context()->packet_size;

    for (unsigned int i = 0; i < m_session.context()->nb_streams; ++i) {
        const AVStream* source = m_session.context()->streams[i];
        if (!source || !source->codecpar) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument("RtpMuxNode SDP snapshot source stream is invalid"));
        }

        AVStream* target = avformat_new_stream(snapshot.get(), nullptr);
        if (!target) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::allocationFailed("avformat_new_stream(rtp sdp snapshot)"));
        }

        const int copyRet = avcodec_parameters_copy(target->codecpar, source->codecpar);
        if (copyRet < 0) {
            return ::media::Result<MediaBufferRef>::failure(
                FFmpegGraphError::fromCode(copyRet, "avcodec_parameters_copy(rtp sdp snapshot)"));
        }

        target->codecpar->codec_tag = 0;
        target->id = source->id;
        target->time_base = source->time_base;
        target->avg_frame_rate = source->avg_frame_rate;
        target->r_frame_rate = source->r_frame_rate;
        target->start_time = source->start_time;
        target->duration = source->duration;
    }

    return FFmpegBufferFactory::wrapOutputFormatContext(std::move(snapshot));
}

::media::Status RtpMuxNode::writeTrailerIfNeeded()
{
    if (!m_session.context() || !m_state.headerWritten() || m_state.trailerWritten()) {
        return ::media::Status::success();
    }
    auto written = RtpMuxProtocolIo::writeTrailer(*m_session.context());
    if (!written) return written;
    m_state.trailerWritten() = true;
    m_state.completion().markTrailerWritten();
    return ::media::Status::success();
}

void RtpMuxNode::releaseRuntimeViews() noexcept
{
    m_pendingStreamConfigs.clear();
    m_pendingPackets.clear();
    m_state.reset();
    m_session.reset();
    m_startupReadyAt = {};
}

bool RtpMuxNode::expectedStreamsRegistered() const noexcept
{
    return (!m_state.expectVideo() && !m_state.expectAudio()) || m_session.streamIndex() != invalidMediaStreamIndex;
}

} // namespace media::ffmpeg::graph
