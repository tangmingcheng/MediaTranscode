#include "internal/graph/runtime/buffer/MediaStartupReleaseTransactionBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"

#include "internal/graph/planner/realtime/MediaStartupSharedObjectPlanner.h"
#include <utility>
#include <atomic>

namespace media::ffmpeg::graph {
namespace {

std::atomic_uint64_t nextReleaseIdentity{1};

} // namespace

::media::Result<MediaBufferRef> MediaStartupReleaseTransactionBuffer::create(
    const MediaStartupSharedObjectPlan& storage, MediaBufferRef release)
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
    return allocateRelease(storage, std::move(release), nextReleaseIdentity.fetch_add(
        1, std::memory_order_relaxed));
}

::media::Result<MediaBufferRef>
MediaStartupReleaseTransactionBuffer::reanchor(
    const MediaStartupReleaseTransactionBuffer& transaction,
    MediaPlaybackActivation activation)
try {
    const auto* release = transaction.release();
    const auto& epoch = activation.epoch();
    if (!release || transaction.releaseIdentity() == 0 ||
        activation != release->activation().reanchor(epoch.masterRelease)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Release reanchor requires the same source epoch and identity"));
    }
    auto* resource = MediaLineageAllocator<MediaAvReleasedUnit>{release->allocationScope()}.resource();
    if (!resource) return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::invalidArgument("Startup release reanchor requires an owned allocation resource"));
    MediaAvReleasedUnits video(release->video().size(), *resource);
    MediaAvReleasedUnits audio(release->audio().size(), *resource);
    for (const auto& unit : release->video()) video.push_back(unit);
    for (const auto& unit : release->audio()) audio.push_back(unit);
    auto rebound = MediaAvStartupReleaseBuffer::create(
        release->storagePlan(), release->groupKey(), release->releaseKind(), std::move(activation),
        std::move(video), std::move(audio),
        release->completedTransitionSequence());
    if (!rebound) return rebound;
    return allocateRelease(transaction.m_storage, std::move(rebound).value(), transaction.releaseIdentity());
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::allocationFailed(
            "Startup release reanchor allocation failed"));
} catch (const std::length_error&) {
    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::allocationFailed("Startup release reanchor size is not representable"));
}

::media::Result<MediaBufferRef>
MediaStartupReleaseTransactionBuffer::createControl(
    const MediaStartupSharedObjectPlan& storage, MediaBufferRef control)
{
    const auto* typed = dynamic_cast<const MediaControlBuffer*>(control.get());
    if (!typed || typed->controlKind() == MediaControlBufferKind::Unknown) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Startup release transaction requires a typed control"));
    }
    if (auto valid = MediaStartupSharedObjectPlanner::validate(storage, MediaStartupSharedObjectRole::ControlTransaction); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    try {
        return ::media::Result<MediaBufferRef>::success(
            std::allocate_shared<MediaStartupReleaseTransactionBuffer>(
                AllocationAllocator{storage.allocation()}, ConstructionKey{}, storage,
                MediaStartupReleaseTransactionKind::Control, std::move(control), 0));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("Startup control transaction allocation failed"));
    }
}

::media::Result<MediaBufferRef>
MediaStartupReleaseTransactionBuffer::allocateRelease(
    const MediaStartupSharedObjectPlan& storage, MediaBufferRef release, std::uint64_t identity)
{
    const auto* typed = dynamic_cast<const MediaAvStartupReleaseBuffer*>(release.get());
    if (!typed || identity == 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("Startup transaction requires an identified release"));
    }
    if (auto valid = MediaStartupSharedObjectPlanner::validate(storage, MediaStartupSharedObjectRole::ReleaseTransaction); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    if (MediaLineageAllocator<std::byte>{storage.allocation()}.resource() !=
        MediaLineageAllocator<std::byte>{typed->allocationScope()}.resource())
        return ::media::Result<MediaBufferRef>::failure(::media::ErrorInfo::invalidArgument(
            "Startup transaction requires its release allocation owner"));
    try {
        return ::media::Result<MediaBufferRef>::success(
            std::allocate_shared<MediaStartupReleaseTransactionBuffer>(
                AllocationAllocator{storage.allocation()},
                ConstructionKey{}, storage, MediaStartupReleaseTransactionKind::Release,
                std::move(release), identity));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("Startup release transaction allocation failed"));
    }
}

MediaStartupReleaseTransactionBuffer::MediaStartupReleaseTransactionBuffer(
    ConstructionKey,
    MediaStartupSharedObjectPlan storage,
    MediaStartupReleaseTransactionKind transactionKind,
    MediaBufferRef payload,
    std::uint64_t releaseIdentity)
    : m_storage(std::move(storage)), m_transactionKind(transactionKind)
    , m_payload(std::move(payload))
    , m_releaseIdentity(releaseIdentity)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName(MediaDiagnosticLiteral{"av_sync.startup_release_transaction"});
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
