#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"

#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

MediaCanonicalAccessUnitBuffer::MediaCanonicalAccessUnitBuffer(
    MediaBufferRef media, MediaScheduledStream stream,
    MediaRunningTime canonicalPresentation,
    std::optional<MediaRunningTime> canonicalDecode,
    MediaRunningTime canonicalDuration, MediaDecodeOrderMode decodeOrder,
    std::uint64_t generation, MediaSourceAccessUnitSequence sourceSequence)
    : m_media(std::move(media)), m_stream(stream),
      m_canonicalPresentation(canonicalPresentation),
      m_canonicalDecode(canonicalDecode), m_canonicalDuration(canonicalDuration),
      m_decodeOrder(decodeOrder), m_generation(generation),
      m_sourceSequence(sourceSequence)
{
    setPayloadKind(MediaPayloadKind::Packet);
    setStreamKind(stream == MediaScheduledStream::Video
                      ? MediaStreamKind::Video : MediaStreamKind::Audio);
}

::media::Result<MediaBufferRef> MediaCanonicalAccessUnitBuffer::create(
    MediaBufferRef media, MediaScheduledStream stream,
    MediaRunningTime canonicalPresentation,
    std::optional<MediaRunningTime> canonicalDecode,
    MediaRunningTime canonicalDuration, MediaDecodeOrderMode decodeOrder,
    std::uint64_t generation, MediaSourceAccessUnitSequence sourceSequence)
{
    const auto expectedStream = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video : MediaStreamKind::Audio;
    if (!FFmpegPacketView::isPacket(media) ||
        media->streamKind() != expectedStream || generation == 0 ||
        sourceSequence.value() == 0 || canonicalDuration.nanoseconds() < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical access unit media contract is incomplete"));
    }
    if (decodeOrder == MediaDecodeOrderMode::ReorderedRequiresDecodeTime &&
        !canonicalDecode) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Reordered canonical access unit requires decode time"));
    }
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaCanonicalAccessUnitBuffer(
            std::move(media), stream, canonicalPresentation, canonicalDecode,
            canonicalDuration, decodeOrder, generation, sourceSequence)));
}

MediaBufferType MediaCanonicalAccessUnitBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

::media::Result<MediaRunningTime>
MediaCanonicalAccessUnitBuffer::canonicalDispatch() const noexcept
{
    if (m_canonicalDecode) {
        return ::media::Result<MediaRunningTime>::success(*m_canonicalDecode);
    }
    if (m_decodeOrder == MediaDecodeOrderMode::PresentationOrderNoReorder) {
        return ::media::Result<MediaRunningTime>::success(m_canonicalPresentation);
    }
    return ::media::Result<MediaRunningTime>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Reordered canonical access unit has no decode time"));
}

} // namespace media::ffmpeg::graph
