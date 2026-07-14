#include "internal/graph/nodes/sync/MediaAvSchedulerHead.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

namespace media::ffmpeg::graph {

::media::Result<MediaAvSchedulerHead> MediaAvSchedulerHead::parse(
    MediaBufferRef buffer, MediaScheduledStream expectedStream)
{
    if (!buffer) return ::media::Result<MediaAvSchedulerHead>::failure(
        ::media::ErrorInfo::invalidArgument("A/V scheduler head is null"));
    if (dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
        return ::media::Result<MediaAvSchedulerHead>::success(
            MediaAvSchedulerHead(std::move(buffer), MediaAvSchedulerHeadKind::Control));
    }
    if (const auto* canonical = dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(
            buffer.get())) {
        if (canonical->stream() != expectedStream) {
            return ::media::Result<MediaAvSchedulerHead>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Canonical scheduler head does not match its port"));
        }
        return ::media::Result<MediaAvSchedulerHead>::success(
            MediaAvSchedulerHead(std::move(buffer), MediaAvSchedulerHeadKind::Canonical));
    }
    if (expectedStream == MediaScheduledStream::Video &&
        dynamic_cast<const MediaVideoRepeatRequestBuffer*>(buffer.get())) {
        return ::media::Result<MediaAvSchedulerHead>::success(
            MediaAvSchedulerHead(std::move(buffer), MediaAvSchedulerHeadKind::VideoRepeat));
    }
    return ::media::Result<MediaAvSchedulerHead>::failure(
        ::media::ErrorInfo::invalidArgument(
            "A/V scheduler head has an unsupported typed payload"));
}

const MediaCanonicalAccessUnitBuffer* MediaAvSchedulerHead::canonical() const noexcept
{
    return dynamic_cast<const MediaCanonicalAccessUnitBuffer*>(m_buffer.get());
}

const MediaVideoRepeatRequestBuffer* MediaAvSchedulerHead::repeat() const noexcept
{
    return dynamic_cast<const MediaVideoRepeatRequestBuffer*>(m_buffer.get());
}

std::uint64_t MediaAvSchedulerHead::generation() const noexcept
{
    return m_kind == MediaAvSchedulerHeadKind::Canonical
        ? canonical()->generation() : repeat()->generation();
}

MediaRunningTime MediaAvSchedulerHead::canonicalPresentation() const noexcept
{
    return m_kind == MediaAvSchedulerHeadKind::Canonical
        ? canonical()->canonicalPresentation() : repeat()->canonicalPresentation();
}

::media::Result<MediaRunningTime>
MediaAvSchedulerHead::canonicalDispatchTime() const noexcept
{
    if (m_kind == MediaAvSchedulerHeadKind::VideoRepeat) {
        return ::media::Result<MediaRunningTime>::success(
            repeat()->canonicalPresentation());
    }
    const auto* unit = canonical();
    return unit->canonicalDispatch();
}

} // namespace media::ffmpeg::graph
