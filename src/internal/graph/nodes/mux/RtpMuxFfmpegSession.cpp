#include "internal/graph/nodes/mux/RtpMuxFfmpegSession.h"

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {

bool RtpMuxFfmpegSession::bindOutput(const MediaBufferRef& buffer) noexcept
{
    auto* format = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!format || !format->context()) return false;
    if (format->ownership() == FFmpegFormatContextOwnership::Output) {
        m_owner = format->takeOutputContext();
        m_context = m_owner.get();
    } else {
        m_owner.reset();
        m_context = format->context();
    }
    m_streamIndex = invalidMediaStreamIndex;
    m_nextPacketDts = AV_NOPTS_VALUE;
    m_packetsWritten = 0;
    m_pacingClock.reset();
    return m_context != nullptr;
}

::media::Status RtpMuxFfmpegSession::registerStreamConfig(
    const MediaBufferRef& buffer,
    MediaStreamKind expectedKind)
{
    if (!m_context) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP mux session requires output context before stream config"));
    }
    if (m_streamIndex != invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP mux session received duplicate stream config"));
    }

    const AVMediaType expectedType = expectedKind == MediaStreamKind::Audio
        ? AVMEDIA_TYPE_AUDIO
        : AVMEDIA_TYPE_VIDEO;
    AVStream* stream = nullptr;
    if (auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get())) {
        AVCodecContext* codec = codecBuffer->context();
        if (!codec || codec->codec_type != expectedType || !validTimeBase(codec->time_base)) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("RTP mux session requires matching encoder context and time_base"));
        }
        stream = avformat_new_stream(m_context, nullptr);
        if (!stream) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream(rtp)"));
        const int copied = avcodec_parameters_from_context(stream->codecpar, codec);
        if (copied < 0) return FFmpegGraphError::statusFromCode(copied, "avcodec_parameters_from_context(rtp)");
        stream->time_base = codec->time_base;
        stream->avg_frame_rate = codec->framerate;
        stream->r_frame_rate = codec->framerate;
    } else if (auto* paramsBuffer = dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get())) {
        const AVCodecParameters* params = paramsBuffer->parameters();
        if (!params || params->codec_type != expectedType || !buffer->timeDescriptor().timeBase.isKnown()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("RTP mux session requires matching codec parameters and time_base"));
        }
        stream = avformat_new_stream(m_context, nullptr);
        if (!stream) return ::media::Status::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream(rtp)"));
        const int copied = avcodec_parameters_copy(stream->codecpar, params);
        if (copied < 0) return FFmpegGraphError::statusFromCode(copied, "avcodec_parameters_copy(rtp)");
        stream->time_base = AVRational{ buffer->timeDescriptor().timeBase.num, buffer->timeDescriptor().timeBase.den };
    } else {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP mux session expected codec configuration"));
    }

    stream->codecpar->codec_tag = 0;
    m_streamIndex = stream->index;
    return ::media::Status::success();
}

::media::Status RtpMuxFfmpegSession::normalizePacketTimestamps(AVPacket& packet, bool enabled)
{
    if (!enabled) return ::media::Status::success();
    if (packet.dts == AV_NOPTS_VALUE && packet.pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP mux session monotonic timestamps require dts or pts"));
    }
    int64_t shift = 0;
    if (m_nextPacketDts != AV_NOPTS_VALUE) {
        if (packet.dts != AV_NOPTS_VALUE && packet.dts < m_nextPacketDts) shift = std::max(shift, m_nextPacketDts - packet.dts);
        if (packet.pts != AV_NOPTS_VALUE && packet.pts < m_nextPacketDts) shift = std::max(shift, m_nextPacketDts - packet.pts);
    }
    if (shift > 0) {
        if (packet.pts != AV_NOPTS_VALUE) {
            if (packet.pts > std::numeric_limits<int64_t>::max() - shift) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RTP mux packet pts overflow"));
            }
            packet.pts += shift;
        }
        if (packet.dts != AV_NOPTS_VALUE) {
            if (packet.dts > std::numeric_limits<int64_t>::max() - shift) {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RTP mux packet dts overflow"));
            }
            packet.dts += shift;
        }
    }
    int64_t normalized = packet.dts != AV_NOPTS_VALUE ? packet.dts : packet.pts;
    if (packet.pts != AV_NOPTS_VALUE && packet.pts > normalized) normalized = packet.pts;
    const int64_t duration = packet.duration > 0 ? packet.duration : 1;
    if (normalized > std::numeric_limits<int64_t>::max() - duration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RTP mux packet timestamp cannot advance past int64 max"));
    }
    m_nextPacketDts = normalized + duration;
    return ::media::Status::success();
}

::media::Result<MediaBufferRef> RtpMuxFfmpegSession::makeSdpFormatSnapshot() const
{
    if (!m_context || m_streamIndex == invalidMediaStreamIndex) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("RTP mux session requires output context and stream before SDP snapshot"));
    }
    AVFormatContext* raw = nullptr;
    const char* url = (m_context->url && m_context->url[0] != '\0') ? m_context->url : nullptr;
    const int allocated = avformat_alloc_output_context2(&raw, nullptr, "rtp", url);
    if (allocated < 0 || !raw) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(allocated < 0 ? allocated : AVERROR_UNKNOWN,
                                       "avformat_alloc_output_context2(rtp sdp snapshot)"));
    }
    ::media::ffmpeg::OutputFormatContextPtr snapshot(raw);
    snapshot->packet_size = m_context->packet_size;
    for (unsigned int i = 0; i < m_context->nb_streams; ++i) {
        const AVStream* source = m_context->streams[i];
        if (!source || !source->codecpar) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument("RTP mux SDP snapshot source stream is invalid"));
        }
        AVStream* target = avformat_new_stream(snapshot.get(), nullptr);
        if (!target) return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::allocationFailed("avformat_new_stream(rtp sdp snapshot)"));
        const int copied = avcodec_parameters_copy(target->codecpar, source->codecpar);
        if (copied < 0) return ::media::Result<MediaBufferRef>::failure(FFmpegGraphError::fromCode(copied, "avcodec_parameters_copy(rtp sdp snapshot)"));
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

void RtpMuxFfmpegSession::reset() noexcept
{
    m_context = nullptr;
    m_owner.reset();
    m_streamIndex = invalidMediaStreamIndex;
    m_nextPacketDts = AV_NOPTS_VALUE;
    m_packetsWritten = 0;
    m_pacingClock.reset();
}

AVFormatContext* RtpMuxFfmpegSession::context() const noexcept { return m_context; }
int& RtpMuxFfmpegSession::streamIndex() noexcept { return m_streamIndex; }
int RtpMuxFfmpegSession::streamIndex() const noexcept { return m_streamIndex; }
int64_t& RtpMuxFfmpegSession::nextPacketDts() noexcept { return m_nextPacketDts; }
std::size_t& RtpMuxFfmpegSession::packetsWritten() noexcept { return m_packetsWritten; }
std::size_t RtpMuxFfmpegSession::packetsWritten() const noexcept { return m_packetsWritten; }
MediaGraphPacingClock& RtpMuxFfmpegSession::pacingClock() noexcept { return m_pacingClock; }
bool RtpMuxFfmpegSession::validTimeBase(AVRational value) noexcept { return value.num != 0 && value.den != 0; }

} // namespace media::ffmpeg::graph
