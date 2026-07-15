#include "internal/graph/sync/MediaCanonicalAccessUnitBuffer.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"

#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

namespace media::ffmpeg::graph {

MediaCanonicalAccessUnitBuffer::MediaCanonicalAccessUnitBuffer(
    MediaBufferRef media, MediaScheduledStream stream,
    std::shared_ptr<const MediaCanonicalLineage> lineage)
    : m_media(std::move(media)), m_stream(stream), m_lineage(std::move(lineage))
{
    setPayloadKind(MediaPayloadKind::Packet);
    setStreamKind(stream == MediaScheduledStream::Video
                      ? MediaStreamKind::Video : MediaStreamKind::Audio);
}

::media::Result<MediaBufferRef> MediaCanonicalAccessUnitBuffer::create(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage)
{
    if (!media || !lineage) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical access unit requires media and immutable lineage"));
    }
    const auto stream = media->streamKind() == MediaStreamKind::Video
        ? MediaScheduledStream::Video : MediaScheduledStream::Audio;
    const auto expectedStream = stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video : MediaStreamKind::Audio;
    if (!FFmpegPacketView::isPacket(media) ||
        media->streamKind() != expectedStream ||
        (media->streamKind() != MediaStreamKind::Video &&
         media->streamKind() != MediaStreamKind::Audio)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical access unit media contract is incomplete"));
    }
    if (auto valid = validateMediaCanonicalLineage(*lineage); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaCanonicalAccessUnitBuffer(
            std::move(media), stream, std::move(lineage))));
}

MediaBufferType MediaCanonicalAccessUnitBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

std::optional<std::uint64_t>
MediaCanonicalAccessUnitBuffer::payloadFootprintBytes() const noexcept
{
    return m_media->payloadFootprintBytes();
}

::media::Result<MediaRunningTime>
MediaCanonicalAccessUnitBuffer::canonicalDispatch() const noexcept
{
    if (m_lineage->decode) {
        return ::media::Result<MediaRunningTime>::success(*m_lineage->decode);
    }
    if (m_lineage->decodeOrder == MediaDecodeOrderMode::PresentationOrderNoReorder) {
        return ::media::Result<MediaRunningTime>::success(m_lineage->presentation);
    }
    return ::media::Result<MediaRunningTime>::failure(
        ::media::ErrorInfo::invalidArgument(
            "Reordered canonical access unit has no decode time"));
}

MediaRunningTime MediaCanonicalAccessUnitBuffer::canonicalPresentation() const noexcept { return m_lineage->presentation; }
const std::optional<MediaRunningTime>& MediaCanonicalAccessUnitBuffer::canonicalDecode() const noexcept { return m_lineage->decode; }
MediaRunningTime MediaCanonicalAccessUnitBuffer::canonicalDuration() const noexcept { return m_lineage->duration; }
MediaDecodeOrderMode MediaCanonicalAccessUnitBuffer::decodeOrder() const noexcept { return m_lineage->decodeOrder; }
std::uint64_t MediaCanonicalAccessUnitBuffer::generation() const noexcept { return m_lineage->generation; }
MediaSourceAccessUnitSequence MediaCanonicalAccessUnitBuffer::sourceSequence() const noexcept { return m_lineage->sourceSequence; }

} // namespace media::ffmpeg::graph
