#include "internal/graph/nodes/mux/FFmpegFileMuxSession.h"

#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
}

#include <utility>

namespace media::ffmpeg::graph {
namespace {

AVRational toAVRational(MediaRational value) noexcept
{
    return AVRational{value.num, value.den};
}

bool known(AVRational value) noexcept
{
    return value.num != 0 && value.den != 0;
}

int streamIndexFor(MediaStreamKind kind, int videoIndex, int audioIndex) noexcept
{
    switch (kind) {
    case MediaStreamKind::Video: return videoIndex;
    case MediaStreamKind::Audio: return audioIndex;
    default: return invalidMediaStreamIndex;
    }
}

::media::Status setStreamIndex(MediaStreamKind kind,
                               int index,
                               int& videoIndex,
                               int& audioIndex)
{
    switch (kind) {
    case MediaStreamKind::Video:
        videoIndex = index;
        return ::media::Status::success();
    case MediaStreamKind::Audio:
        audioIndex = index;
        return ::media::Status::success();
    default:
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession stream config must be video or audio"));
    }
}

AVRational packetTimeBase(const MediaBufferRef& buffer) noexcept
{
    if (!buffer) return AVRational{0, 1};
    if (buffer->timeDescriptor().timeBase.isKnown()) {
        return toAVRational(buffer->timeDescriptor().timeBase);
    }
    if (buffer->formatDescriptor().time.timeBase.isKnown()) {
        return toAVRational(buffer->formatDescriptor().time.timeBase);
    }
    return AVRational{0, 1};
}

} // namespace

FFmpegFileMuxSession::FFmpegFileMuxSession(bool expectVideo, bool expectAudio) noexcept
    : m_expectVideo(expectVideo)
    , m_expectAudio(expectAudio)
{
}

FFmpegFileMuxSession::~FFmpegFileMuxSession()
{
    abort();
}

::media::Status FFmpegFileMuxSession::bindResource(MediaGraphExecutionContext&,
                                                    const MediaBufferRef& buffer)
{
    if (m_terminalFailure) return terminalStatus();
    if (m_finished) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession cannot bind resource after finish")));
    }
    if (m_outputContext) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession received duplicate output resource")));
    }
    auto* contextBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!contextBuffer || !contextBuffer->context()) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession requires output format context")));
    }
    if (contextBuffer->ownership() == FFmpegFormatContextOwnership::Output) {
        m_outputContextOwner = contextBuffer->takeOutputContext();
        m_outputContext = m_outputContextOwner.get();
    } else {
        m_outputContext = contextBuffer->context();
    }
    if (!m_outputContext) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FFmpegFileMuxSession output context transfer failed")));
    }
    auto configs = registerPendingStreamConfigs();
    return configs ? writePendingPacketsIfReady() : configs;
}

::media::Status FFmpegFileMuxSession::bindStreamConfig(MediaGraphExecutionContext&,
                                                        const MediaBufferRef& buffer)
{
    if (m_terminalFailure) return terminalStatus();
    if (m_finished || m_headerWritten) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession received late stream config")));
    }
    if (!dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get()) &&
        !dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get())) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession expected stream config")));
    }
    auto planned = validatePlannedStream(buffer->streamKind());
    if (!planned) return preserve(planned);
    if (!m_outputContext) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    auto registered = registerStreamFromConfig(buffer);
    return registered ? writePendingPacketsIfReady() : registered;
}

::media::Status FFmpegFileMuxSession::write(MediaGraphExecutionContext&,
                                             const MediaBufferRef& buffer)
{
    if (m_terminalFailure) return terminalStatus();
    if (m_finished) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession cannot write after finish")));
    }
    if (!FFmpegPacketView::isPacket(buffer)) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession requires packet input")));
    }
    auto header = writeHeaderIfReady();
    if (!header) return header;
    if (!m_headerWritten) {
        m_pendingPackets.push_back(buffer);
        return ::media::Status::success();
    }
    auto pending = writePendingPacketsIfReady();
    return pending ? preserve(writePacketNow(buffer)) : pending;
}

::media::Result<MediaMuxSessionPollResult> FFmpegFileMuxSession::poll(
    MediaGraphExecutionContext&)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaMuxSessionPollResult>::failure(*m_terminalFailure);
    }
    return ::media::Result<MediaMuxSessionPollResult>::success({false, std::nullopt});
}

bool FFmpegFileMuxSession::hasPendingOutput() const noexcept
{
    return false;
}

bool FFmpegFileMuxSession::bindingsReady() const noexcept
{
    return m_headerWritten && !m_terminalFailure;
}

::media::Status FFmpegFileMuxSession::flush(MediaGraphExecutionContext&)
{
    if (m_terminalFailure) return terminalStatus();
    return writePendingPacketsIfReady();
}

::media::Status FFmpegFileMuxSession::finish(MediaGraphExecutionContext&)
{
    if (m_terminalFailure) return terminalStatus();
    if (m_finished) return ::media::Status::success();
    if (!m_outputContext) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FFmpegFileMuxSession cannot finish without output resource")));
    }
    auto configs = registerPendingStreamConfigs();
    if (!configs) return configs;
    auto header = writeHeaderIfReady();
    if (!header) return header;
    if (!m_headerWritten) {
        return preserve(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FFmpegFileMuxSession cannot finish before expected streams are registered")));
    }
    auto pending = writePendingPacketsIfReady();
    if (!pending) return pending;
    auto trailer = writeTrailerIfNeeded();
    if (!trailer) return trailer;
    m_finished = true;
    return ::media::Status::success();
}

void FFmpegFileMuxSession::abort() noexcept
{
    release();
    m_finished = true;
}

::media::Status FFmpegFileMuxSession::validatePlannedStream(MediaStreamKind kind) const
{
    switch (kind) {
    case MediaStreamKind::Video:
        if (m_expectVideo) return ::media::Status::success();
        break;
    case MediaStreamKind::Audio:
        if (m_expectAudio) return ::media::Status::success();
        break;
    default:
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession stream config must be video or audio"));
    }
    return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "FFmpegFileMuxSession received stream config excluded by the mux plan"));
}

::media::Status FFmpegFileMuxSession::registerPendingStreamConfigs()
{
    if (!m_outputContext || m_pendingStreamConfigs.empty()) {
        return ::media::Status::success();
    }
    auto pending = std::move(m_pendingStreamConfigs);
    m_pendingStreamConfigs.clear();
    for (const auto& buffer : pending) {
        auto status = registerStreamFromConfig(buffer);
        if (!status) return status;
    }
    return ::media::Status::success();
}

::media::Status FFmpegFileMuxSession::registerStreamFromConfig(const MediaBufferRef& buffer)
{
    if (!m_outputContext) {
        m_pendingStreamConfigs.push_back(buffer);
        return ::media::Status::success();
    }
    if (dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get())) {
        return preserve(registerStreamFromCodecContext(buffer));
    }
    if (dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get())) {
        return preserve(registerStreamFromCodecParameters(buffer));
    }
    return preserve(::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "FFmpegFileMuxSession expected stream config")));
}

::media::Status FFmpegFileMuxSession::registerStreamFromCodecContext(
    const MediaBufferRef& buffer)
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    AVCodecContext* codec = codecBuffer ? codecBuffer->context() : nullptr;
    if (!codec || !known(codec->time_base)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession requires codec context and time_base"));
    }
    const MediaStreamKind kind = buffer->streamKind();
    if (kind != MediaStreamKind::Video && kind != MediaStreamKind::Audio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession codec context must be video or audio"));
    }
    if (streamIndexFor(kind, m_videoStreamIndex, m_audioStreamIndex) != invalidMediaStreamIndex) {
        return ::media::Status::success();
    }
    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("avformat_new_stream"));
    }
    const int ret = avcodec_parameters_from_context(stream->codecpar, codec);
    if (ret < 0) return FFmpegGraphError::statusFromCode(ret, "avcodec_parameters_from_context");
    stream->codecpar->codec_tag = 0;
    stream->time_base = codec->time_base;
    if (kind == MediaStreamKind::Video) {
        stream->avg_frame_rate = codec->framerate;
        stream->r_frame_rate = codec->framerate;
    }
    return setStreamIndex(kind, stream->index, m_videoStreamIndex, m_audioStreamIndex);
}

::media::Status FFmpegFileMuxSession::registerStreamFromCodecParameters(
    const MediaBufferRef& buffer)
{
    auto* paramsBuffer = dynamic_cast<FFmpegCodecParametersBuffer*>(buffer.get());
    const AVCodecParameters* params = paramsBuffer ? paramsBuffer->parameters() : nullptr;
    if (!params || !buffer->timeDescriptor().timeBase.isKnown()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession requires codec parameters and time_base"));
    }
    const MediaStreamKind kind = buffer->streamKind();
    if (kind != MediaStreamKind::Video && kind != MediaStreamKind::Audio) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession codec parameters must be video or audio"));
    }
    if (streamIndexFor(kind, m_videoStreamIndex, m_audioStreamIndex) != invalidMediaStreamIndex) {
        return ::media::Status::success();
    }
    AVStream* stream = avformat_new_stream(m_outputContext, nullptr);
    if (!stream) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("avformat_new_stream"));
    }
    const int ret = avcodec_parameters_copy(stream->codecpar, params);
    if (ret < 0) return FFmpegGraphError::statusFromCode(ret, "avcodec_parameters_copy");
    stream->codecpar->codec_tag = 0;
    stream->time_base = toAVRational(buffer->timeDescriptor().timeBase);
    return setStreamIndex(kind, stream->index, m_videoStreamIndex, m_audioStreamIndex);
}

::media::Status FFmpegFileMuxSession::writeHeaderIfReady()
{
    if (m_headerWritten) return ::media::Status::success();
    if (!m_outputContext || m_outputContext->nb_streams == 0 || !expectedStreamsRegistered()) {
        return ::media::Status::success();
    }
    const int ret = avformat_write_header(m_outputContext, nullptr);
    return ret < 0
        ? preserve(FFmpegGraphError::statusFromCode(ret, "avformat_write_header"))
        : (m_headerWritten = true, ::media::Status::success());
}

::media::Status FFmpegFileMuxSession::writePendingPacketsIfReady()
{
    auto header = writeHeaderIfReady();
    if (!header || !m_headerWritten || m_pendingPackets.empty()) return header;
    auto pending = std::move(m_pendingPackets);
    m_pendingPackets.clear();
    for (const auto& buffer : pending) {
        auto status = writePacketNow(buffer);
        if (!status) return preserve(status);
    }
    return ::media::Status::success();
}

::media::Status FFmpegFileMuxSession::writePacketNow(const MediaBufferRef& buffer)
{
    const AVPacket* source = FFmpegPacketView::packet(buffer);
    if (!m_outputContext || !source) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession requires output context and packet"));
    }
    const int targetIndex = streamIndexFor(
        buffer->streamKind(), m_videoStreamIndex, m_audioStreamIndex);
    if (targetIndex == invalidMediaStreamIndex ||
        targetIndex >= static_cast<int>(m_outputContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession packet stream is not registered"));
    }
    AVStream* muxStream = m_outputContext->streams[targetIndex];
    const AVRational sourceTimeBase = packetTimeBase(buffer);
    const AVRational muxTimeBase = muxStream ? muxStream->time_base : AVRational{0, 1};
    if (!known(sourceTimeBase) || !known(muxTimeBase)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpegFileMuxSession requires packet time_base"));
    }
    auto packet = ::media::ffmpeg::makePacket();
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::allocationFailed("av_packet_alloc"));
    }
    const int refRet = av_packet_ref(packet.get(), source);
    if (refRet < 0) return FFmpegGraphError::statusFromCode(refRet, "av_packet_ref");
    av_packet_rescale_ts(packet.get(), sourceTimeBase, muxTimeBase);
    packet->stream_index = targetIndex;
    const int writeRet = av_interleaved_write_frame(m_outputContext, packet.get());
    return writeRet < 0
        ? FFmpegGraphError::statusFromCode(writeRet, "av_interleaved_write_frame")
        : ::media::Status::success();
}

::media::Status FFmpegFileMuxSession::writeTrailerIfNeeded()
{
    if (m_trailerWritten) return ::media::Status::success();
    const int ret = av_write_trailer(m_outputContext);
    if (ret < 0) return preserve(FFmpegGraphError::statusFromCode(ret, "av_write_trailer"));
    m_trailerWritten = true;
    return ::media::Status::success();
}

::media::Status FFmpegFileMuxSession::preserve(::media::Status status)
{
    if (!status && !m_terminalFailure) m_terminalFailure = status.error();
    return m_terminalFailure ? terminalStatus() : status;
}

::media::Status FFmpegFileMuxSession::terminalStatus() const
{
    return ::media::Status::failure(*m_terminalFailure);
}

bool FFmpegFileMuxSession::expectedStreamsRegistered() const noexcept
{
    return (!m_expectVideo || m_videoStreamIndex != invalidMediaStreamIndex) &&
           (!m_expectAudio || m_audioStreamIndex != invalidMediaStreamIndex);
}

void FFmpegFileMuxSession::release() noexcept
{
    m_pendingStreamConfigs.clear();
    m_pendingPackets.clear();
    m_outputContext = nullptr;
    m_outputContextOwner.reset();
}

} // namespace media::ffmpeg::graph
