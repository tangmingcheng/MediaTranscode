#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

#include <utility>
#include <atomic>

namespace media::ffmpeg::graph {
namespace {

std::atomic_uint64_t nextReleaseIdentity{1};

} // namespace

::media::Result<MediaBufferRef> MediaStartupReleaseTransactionBuffer::create(
    MediaBufferRef release)
{
    const auto* typed = dynamic_cast<const MediaAvStartupReleaseBuffer*>(
        release.get());
    if (!typed || !typed->groupKey().valid()) {
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
            std::move(release), nextReleaseIdentity.fetch_add(
                1, std::memory_order_relaxed))));
}

::media::Result<MediaBufferRef>
MediaStartupReleaseTransactionBuffer::reanchor(
    const MediaStartupReleaseTransactionBuffer& transaction,
    MediaPlaybackActivation activation)
{
    const auto* release = transaction.release();
    const auto& epoch = activation.epoch();
    if (!release || transaction.releaseIdentity() == 0 ||
        activation != release->activation().reanchor(epoch.masterRelease)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Release reanchor requires the same source epoch and identity"));
    }
    auto rebound = MediaAvStartupReleaseBuffer::create(
        release->groupKey(), release->releaseKind(), std::move(activation),
        release->video(), release->audio(),
        release->completedTransitionSequence());
    if (!rebound) return rebound;
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaStartupReleaseTransactionBuffer(
            MediaStartupReleaseTransactionKind::Release,
            std::move(rebound).value(), transaction.releaseIdentity())));
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
            std::move(control), 0)));
}

MediaStartupReleaseTransactionBuffer::MediaStartupReleaseTransactionBuffer(
    MediaStartupReleaseTransactionKind transactionKind,
    MediaBufferRef payload,
    std::uint64_t releaseIdentity)
    : m_transactionKind(transactionKind)
    , m_payload(std::move(payload))
    , m_releaseIdentity(releaseIdentity)
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

std::uint64_t
MediaStartupReleaseTransactionBuffer::releaseIdentity() const noexcept
{
    return m_releaseIdentity;
}

} // namespace media::ffmpeg::graph
