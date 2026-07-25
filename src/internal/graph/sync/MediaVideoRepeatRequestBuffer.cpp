#include "internal/graph/sync/MediaVideoRepeatRequestBuffer.h"

namespace media::ffmpeg::graph {

MediaVideoRepeatRequestBuffer::MediaVideoRepeatRequestBuffer(
    MediaRunningTime presentation, MediaRunningTime duration,
    std::uint64_t generation, MediaVideoRepeatRequestId requestId)
    : m_presentation(presentation), m_duration(duration),
      m_generation(generation), m_requestId(requestId)
{
    setStreamKind(MediaStreamKind::Video);
    // Repeat requests travel on the scheduler's encoded-packet control lane;
    // their C++ type distinguishes them from canonical access units.
    setPayloadKind(MediaPayloadKind::Packet);
}

::media::Result<MediaBufferRef> MediaVideoRepeatRequestBuffer::create(
    MediaRunningTime presentation, MediaRunningTime duration,
    std::uint64_t generation, MediaVideoRepeatRequestId requestId)
{
    if (generation == 0 || requestId.value() == 0 || duration.nanoseconds() <= 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("Video repeat request is incomplete"));
    }
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaVideoRepeatRequestBuffer(
            presentation, duration, generation, requestId)));
}

MediaBufferType MediaVideoRepeatRequestBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

} // namespace media::ffmpeg::graph
