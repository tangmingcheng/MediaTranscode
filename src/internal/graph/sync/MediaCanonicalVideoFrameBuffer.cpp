#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

namespace media::ffmpeg::graph {

MediaCanonicalVideoFrameBuffer::MediaCanonicalVideoFrameBuffer(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage)
    : m_media(std::move(media)), m_lineage(std::move(lineage))
{
    setStreamKind(MediaStreamKind::Video);
    setPayloadKind(MediaPayloadKind::Frame);
}

::media::Result<MediaBufferRef> MediaCanonicalVideoFrameBuffer::create(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage)
{
    if (!media || media->type() != MediaBufferType::Frame ||
        media->streamKind() != MediaStreamKind::Video || !lineage) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical video frame requires a video frame and lineage"));
    }
    if (auto valid = validateMediaCanonicalLineage(*lineage); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaCanonicalVideoFrameBuffer(std::move(media), std::move(lineage))));
}

MediaBufferType MediaCanonicalVideoFrameBuffer::type() const noexcept { return MediaBufferType::Event; }

} // namespace media::ffmpeg::graph
