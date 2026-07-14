#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegSession.h"

#include "internal/graph/nodes/mux/ScheduledRtpMuxFfmpegOptions.h"

#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

ScheduledRtpMuxFfmpegSession::ScheduledRtpMuxFfmpegSession(
    FFmpegDatagramSink sink)
    : m_sink(std::move(sink))
{
}

ScheduledRtpMuxFfmpegSession::~ScheduledRtpMuxFfmpegSession()
{
    releaseOutput();
}

::media::Status ScheduledRtpMuxFfmpegSession::configure(
    ScheduledRtpMuxStreamConfig config)
{
    if (m_state != State::Empty || m_config) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP mux received duplicate configuration"));
    }
    if (!m_sink) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP mux requires a datagram sink"));
    }
    m_config.emplace(std::move(config));
    try {
        m_rewriteScratch.reserve(static_cast<std::size_t>(
            m_config->avioConfig().maximumDatagramBytes()));
    } catch (const std::bad_alloc&) {
        m_config.reset();
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "scheduled RTP rewrite scratch"));
    }
    m_state = State::Configured;
    return ::media::Status::success();
}

::media::Status ScheduledRtpMuxFfmpegSession::open()
{
    if (m_state != State::Configured || !m_config || m_context || m_avio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP mux must be configured exactly once before open"));
    }

    AVFormatContext* context = nullptr;
    const int allocation = avformat_alloc_output_context2(
        &context, nullptr, "rtp", nullptr);
    if (allocation < 0 || !context) {
        return ::media::Status::failure(
            FFmpegGraphError::fromCode(
                allocation < 0 ? allocation : AVERROR(ENOMEM),
                "avformat_alloc_output_context2(scheduled rtp)"));
    }
    m_context = context;
    m_context->packet_size = m_config->avioConfig().maximumDatagramBytes();
    m_context->flags |= AVFMT_FLAG_CUSTOM_IO;

    AVStream* stream = avformat_new_stream(m_context, nullptr);
    if (!stream) {
        releaseOutput();
        m_state = State::Configured;
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "avformat_new_stream(scheduled rtp)"));
    }
    const int copied = avcodec_parameters_copy(
        stream->codecpar, &m_config->codecParameters());
    if (copied < 0) {
        releaseOutput();
        m_state = State::Configured;
        return FFmpegGraphError::statusFromCode(
            copied, "avcodec_parameters_copy(scheduled rtp stream)");
    }
    stream->codecpar->codec_tag = 0;
    stream->time_base = m_config->streamTimeBase();

    auto avio = FFmpegDatagramWriteAvio::create(
        m_config->avioConfig(),
        [this](std::span<const std::uint8_t> datagram) {
            return emitMuxDatagram(datagram);
        });
    if (!avio) {
        releaseOutput();
        m_state = State::Configured;
        return ::media::Status::failure(avio.error());
    }
    m_avio = std::move(avio.value());
    auto opened = m_avio->open();
    if (!opened) {
        releaseOutput();
        m_state = State::Configured;
        return opened;
    }
    m_context->pb = m_avio->context();
    if (m_context->packet_size != m_context->pb->max_packet_size ||
        m_context->packet_size <= 12) {
        releaseOutput();
        m_state = State::Configured;
        return ::media::Status::failure(
            ::media::ErrorInfo::internalError(
                "scheduled RTP packet size contract is inconsistent"));
    }

    auto options = ScheduledRtpMuxFfmpegOptions::apply(
        m_context->priv_data, *m_config);
    if (!options) {
        releaseOutput();
        m_state = State::Configured;
        return options;
    }
    auto verified = ScheduledRtpMuxFfmpegOptions::verify(
        m_context->priv_data, *m_config);
    if (!verified) {
        releaseOutput();
        m_state = State::Configured;
        return verified;
    }
    const int header = avformat_write_header(m_context, nullptr);
    if (auto callback = preserveCallbackFailure(); !callback) {
        return callback;
    }
    if (header < 0) {
        releaseOutput();
        m_state = State::Configured;
        return FFmpegGraphError::statusFromCode(
            header, "avformat_write_header(scheduled rtp)");
    }
    m_state = State::Open;
    return ::media::Status::success();
}

::media::Status ScheduledRtpMuxFfmpegSession::writeAccessUnit(
    const AVPacket& packet,
    MediaRtpTimestamp timestamp)
{
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_state != State::Open || !m_context || !m_avio ||
        !packet.data || packet.size <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP mux requires an open session and non-empty access unit"));
    }
    auto copy = ::media::ffmpeg::makePacket();
    if (!copy) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("scheduled RTP access-unit packet"));
    }
    const int referenced = av_packet_ref(copy.get(), &packet);
    if (referenced < 0) {
        return FFmpegGraphError::statusFromCode(
            referenced, "av_packet_ref(scheduled rtp)" );
    }
    copy->stream_index = 0;
    m_activeTimestamp = timestamp;
    const int written = av_write_frame(m_context, copy.get());
    m_activeTimestamp.reset();
    if (auto callback = preserveCallbackFailure(); !callback) {
        return callback;
    }
    if (written < 0) {
        return FFmpegGraphError::statusFromCode(
            written, "av_write_frame(scheduled rtp)");
    }
    return ::media::Status::success();
}

::media::Status ScheduledRtpMuxFfmpegSession::writeTrailer()
{
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_state != State::Open || !m_context || !m_avio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "scheduled RTP trailer requires an open session"));
    }
    const int trailer = av_write_trailer(m_context);
    if (auto callback = preserveCallbackFailure(); !callback) {
        return callback;
    }
    if (trailer < 0) {
        return FFmpegGraphError::statusFromCode(
            trailer, "av_write_trailer(scheduled rtp)");
    }
    releaseOutput();
    m_state = State::TrailerWritten;
    return ::media::Status::success();
}

::media::Status ScheduledRtpMuxFfmpegSession::reset() noexcept
{
    releaseOutput();
    m_config.reset();
    m_activeTimestamp.reset();
    m_terminalFailure.reset();
    m_state = State::Empty;
    return ::media::Status::success();
}

::media::Status ScheduledRtpMuxFfmpegSession::emitMuxDatagram(
    std::span<const std::uint8_t> datagram)
{
    if (!m_activeTimestamp || !m_config) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg emitted RTP without an active access-unit clock mapping"));
    }
    const MediaRtpDatagramRewriteParameters parameters(
        m_config->identity(), *m_activeTimestamp);
    auto rewritten = MediaRtpDatagramRewriter::rewrite(
        datagram, parameters, m_rewriteScratch);
    if (!rewritten) {
        return rewritten;
    }
    return m_sink(m_rewriteScratch);
}

::media::Status ScheduledRtpMuxFfmpegSession::preserveCallbackFailure()
{
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_avio && m_avio->sinkFailure()) {
        poison(*m_avio->sinkFailure());
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_context && m_context->pb && m_context->pb->error < 0) {
        poison(FFmpegGraphError::fromCode(
            m_context->pb->error, "scheduled RTP custom AVIO"));
        return ::media::Status::failure(*m_terminalFailure);
    }
    return ::media::Status::success();
}

void ScheduledRtpMuxFfmpegSession::poison(::media::ErrorInfo error)
{
    if (!m_terminalFailure) {
        m_terminalFailure = std::move(error);
    }
    m_state = State::Poisoned;
}

void ScheduledRtpMuxFfmpegSession::releaseOutput() noexcept
{
    if (m_context) {
        m_context->pb = nullptr;
    }
    if (m_avio) {
        if (m_avio->context()) {
            (void)m_avio->close();
        }
        m_avio.reset();
    }
    if (m_context) {
        avformat_free_context(m_context);
        m_context = nullptr;
    }
}

} // namespace media::ffmpeg::graph
