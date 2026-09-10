#include "internal/graph/runtime/threading/MediaRuntimeReclamationOwner.h"

#include <new>
#include <system_error>

namespace media::ffmpeg::graph {

::media::Result<std::unique_ptr<MediaRuntimeReclamationOwner>>
MediaRuntimeReclamationOwner::create(void* target, Reclaim reclaim)
{
    using Result = ::media::Result<std::unique_ptr<MediaRuntimeReclamationOwner>>;
    if (!target || !reclaim) return Result::failure(::media::ErrorInfo::invalidArgument(
        "reclamation owner requires a retained target and single-shot operation"));
    try {
        auto owner = std::unique_ptr<MediaRuntimeReclamationOwner>(
            new MediaRuntimeReclamationOwner(target, reclaim));
        owner->m_thread = std::thread([instance = owner.get()] { instance->run(); });
        return Result::success(std::move(owner));
    } catch (const std::system_error& error) {
        return Result::failure(::media::ErrorInfo::internalError(error.what()));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed("reclamation owner"));
    }
}

MediaRuntimeReclamationOwner::~MediaRuntimeReclamationOwner()
{
    {
        std::lock_guard lock(m_mutex);
        m_cancelled = true;
    }
    m_ready.notify_one();
    join();
}

void MediaRuntimeReclamationOwner::request() noexcept
{
    {
        std::lock_guard lock(m_mutex);
        m_requested = true;
    }
    m_ready.notify_one();
}

void MediaRuntimeReclamationOwner::run() noexcept
{
    {
        std::unique_lock lock(m_mutex);
        m_ready.wait(lock, [this] { return m_requested || m_cancelled; });
        if (!m_requested) return;
    }
    m_reclaim(m_target);
    m_completed.store(true, std::memory_order_release);
}

void MediaRuntimeReclamationOwner::join()
{
    if (m_thread.joinable()) m_thread.join();
}

std::uint64_t MediaRuntimeReclamationOwner::fixedStorageBytes() noexcept
{
    // Native thread stacks and implementation control blocks remain explicitly
    // observed platform allocations, not fabricated portable sizeof estimates.
    return sizeof(MediaRuntimeReclamationOwner);
}

} // namespace media::ffmpeg::graph
