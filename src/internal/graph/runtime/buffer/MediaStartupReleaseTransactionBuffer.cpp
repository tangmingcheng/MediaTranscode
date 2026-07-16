#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaStartupReleaseTransactionBuffer::create(
    MediaBufferRef release)
{
    const auto* typed = dynamic_cast<const MediaAvStartupReleaseBuffer*>(
        release.get());
    if (!typed || !typed->groupKey().valid() ||
        typed->epoch().generation == 0 ||
        typed->audioOrigin().generation != typed->epoch().generation ||
        typed->audioOrigin().sourceStart != typed->epoch().sourceStart ||
        typed->audioOrigin().masterRelease != typed->epoch().masterRelease) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Startup release transaction requires a complete typed release"));
    }
    if (auto status = MediaAvStartupReleaseBuffer::validateReleaseKind(
            typed->releaseKind()); !status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaStartupReleaseTransactionBuffer(
            MediaStartupReleaseTransactionKind::Release,
            std::move(release))));
}

::media::Result<MediaBufferRef>
MediaStartupReleaseTransactionBuffer::createControl(MediaBufferRef control)
{
    const auto* typed = dynamic_cast<const MediaControlBuffer*>(control.get());
    if (!typed || typed->controlKind() == MediaControlBufferKind::Unknown) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Startup release transaction requires a typed control"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaStartupReleaseTransactionBuffer(
            MediaStartupReleaseTransactionKind::Control,
            std::move(control))));
}

MediaStartupReleaseTransactionBuffer::MediaStartupReleaseTransactionBuffer(
    MediaStartupReleaseTransactionKind transactionKind,
    MediaBufferRef payload)
    : m_transactionKind(transactionKind)
    , m_payload(std::move(payload))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("av_sync.startup_release_transaction");
}

MediaBufferType MediaStartupReleaseTransactionBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

MediaStartupReleaseTransactionKind
MediaStartupReleaseTransactionBuffer::transactionKind() const noexcept
{
    return m_transactionKind;
}

const MediaBufferRef& MediaStartupReleaseTransactionBuffer::payload() const noexcept
{
    return m_payload;
}

const MediaAvStartupReleaseBuffer*
MediaStartupReleaseTransactionBuffer::release() const noexcept
{
    return m_transactionKind == MediaStartupReleaseTransactionKind::Release
        ? dynamic_cast<const MediaAvStartupReleaseBuffer*>(m_payload.get())
        : nullptr;
}

const MediaControlBuffer*
MediaStartupReleaseTransactionBuffer::control() const noexcept
{
    return m_transactionKind == MediaStartupReleaseTransactionKind::Control
        ? dynamic_cast<const MediaControlBuffer*>(m_payload.get())
        : nullptr;
}

} // namespace media::ffmpeg::graph
