#pragma once

#include "media_transcode/Result.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace media::ffmpeg::graph {

// One pre-created owner, one lifetime command. No shared queue can let a slow
// driver prevent another segment from completing its own physical retirement.
class MediaRuntimeReclamationOwner final {
public:
    using Reclaim = void (*)(void*) noexcept;
    static ::media::Result<std::unique_ptr<MediaRuntimeReclamationOwner>> create(
        void* target, Reclaim reclaim);
    ~MediaRuntimeReclamationOwner();
    MediaRuntimeReclamationOwner(const MediaRuntimeReclamationOwner&) = delete;
    MediaRuntimeReclamationOwner& operator=(const MediaRuntimeReclamationOwner&) = delete;

    void request() noexcept;
    bool completed() const noexcept { return m_completed.load(std::memory_order_acquire); }
    void markProgress() noexcept { m_progress.fetch_add(1, std::memory_order_release); }
    std::uint64_t progress() const noexcept { return m_progress.load(std::memory_order_acquire); }
    void join();
    static std::uint64_t fixedStorageBytes() noexcept;

private:
    MediaRuntimeReclamationOwner(void* target, Reclaim reclaim) noexcept
        : m_target(target), m_reclaim(reclaim) {}
    void run() noexcept;

    void* m_target;
    Reclaim m_reclaim;
    std::mutex m_mutex;
    std::condition_variable m_ready;
    std::thread m_thread;
    bool m_requested = false;
    bool m_cancelled = false;
    std::atomic_bool m_completed{false};
    std::atomic<std::uint64_t> m_progress{0};
};

} // namespace media::ffmpeg::graph
