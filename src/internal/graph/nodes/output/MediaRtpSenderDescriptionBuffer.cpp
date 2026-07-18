#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaRtpSenderDescriptionBuffer::create(
    MediaScheduledStream stream,
    std::uint64_t generation,
    MediaSdpSessionIdentity session,
    MediaRtpSdpMediaDescription media)
{
    const auto expectedKind = stream == MediaScheduledStream::Video
        ? MediaSdpMediaKind::Video
        : MediaSdpMediaKind::Audio;
    if (generation == 0 || session.sessionVersion() != generation ||
        media.identity().kind() != expectedKind ||
        media.identity().ssrc() == 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP sender description does not match its active stream generation"));
    }
    auto* buffer = new (std::nothrow) MediaRtpSenderDescriptionBuffer(
        stream, generation, std::move(session), std::move(media));
    if (!buffer) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaRtpSenderDescriptionBuffer"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(buffer));
}

MediaRtpSenderDescriptionBuffer::MediaRtpSenderDescriptionBuffer(
    MediaScheduledStream stream,
    std::uint64_t generation,
    MediaSdpSessionIdentity session,
    MediaRtpSdpMediaDescription media)
    : m_stream(stream),
      m_generation(generation),
      m_session(std::move(session)),
      m_media(std::move(media))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName(stream == MediaScheduledStream::Video
        ? "rtp.video.sender_opened"
        : "rtp.audio.sender_opened");
}

MediaBufferType MediaRtpSenderDescriptionBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

} // namespace media::ffmpeg::graph
