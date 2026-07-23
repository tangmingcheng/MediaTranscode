#include "internal/graph/sync/MediaCanonicalVideoFrameBuffer.h"

namespace media::ffmpeg::graph {

MediaCanonicalVideoFrameBuffer::MediaCanonicalVideoFrameBuffer(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage)
    : m_media(std::move(media)), m_lineage(std::move(lineage))
{
    setStreamKind(MediaStreamKind::Video);
    setPayloadKind(MediaPayloadKind::Frame);
    setFormatDescriptor(m_media->formatDescriptor());
    setTimeDescriptor(m_media->timeDescriptor());
    setHardwareDescriptor(m_media->hardwareDescriptor());
    setTimestamps(m_media->pts(), m_media->dts(), m_media->duration());
    setFlags(m_media->flags());
}

::media::Result<MediaBufferRef> MediaCanonicalVideoFrameBuffer::create(
    MediaBufferRef media, std::shared_ptr<const MediaCanonicalLineage> lineage)
{
    const bool framePayload = media &&
        (media->type() == MediaBufferType::Frame ||
         media->type() == MediaBufferType::HardwareFrame);
    if (!framePayload ||
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
