#pragma once

#include "internal/graph/runtime/resource/MediaByteReservationCounter.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <memory>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaPreparationStorageBudget;

class MediaPreparationStorageLease final {
public:
    ~MediaPreparationStorageLease();
    MediaPreparationStorageLease(const MediaPreparationStorageLease&) = delete;
    MediaPreparationStorageLease& operator=(const MediaPreparationStorageLease&) = delete;
    MediaPreparationStorageLease(MediaPreparationStorageLease&& other) noexcept;
    MediaPreparationStorageLease& operator=(MediaPreparationStorageLease&& other) noexcept;
    std::size_t bytes() const noexcept { return m_bytes; }
    explicit operator bool() const noexcept { return m_owner != nullptr; }

private:
    friend class MediaPreparationStorageBudget;
    MediaPreparationStorageLease(
        std::shared_ptr<MediaPreparationStorageBudget> owner,
        std::size_t bytes) noexcept;
    void release() noexcept;
    std::shared_ptr<MediaPreparationStorageBudget> m_owner;
    std::size_t m_bytes = 0;
};

struct MediaPreparationStorageSnapshot final {
    std::size_t capacity;
    std::size_t retainedBytes;
    std::size_t highWaterBytes;
};

// Logical engine-owned preparation storage, separate from raw wire probe size
// and from a runtime DAG producer. Consumers must honor their typed size plan.
class MediaPreparationStorageBudget final
    : public std::enable_shared_from_this<MediaPreparationStorageBudget> {
public:
    static ::media::Result<std::shared_ptr<MediaPreparationStorageBudget>> create(
        std::size_t capacity);
    ::media::Result<MediaPreparationStorageLease> reserve(std::size_t bytes);
    MediaPreparationStorageSnapshot snapshot() const noexcept;

private:
    friend class MediaPreparationStorageLease;
    explicit MediaPreparationStorageBudget(std::size_t capacity) noexcept;
    void release(std::size_t bytes) noexcept;

    mutable std::mutex m_mutex;
    MediaByteReservationCounter m_storage;
    std::size_t m_highWaterBytes = 0;
};

} // namespace media::ffmpeg::graph
