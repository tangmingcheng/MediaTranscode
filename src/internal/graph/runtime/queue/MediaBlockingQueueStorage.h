#pragma once

#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"
#include "media_transcode/Result.h"

#include "internal/graph/runtime/storage/MediaLineageAllocationScope.h"
#include <cstddef>
#include <deque>
#include <span>

namespace media::ffmpeg::graph {

class MediaBlockingQueueStorage final {
public:
    using PreparationAllocator = void (*) (
        MediaFixedCapacityStorage<MediaBufferRef>& buffers,
        std::size_t capacity, std::pmr::memory_resource& resource);

    class PreparedPush final {
    public:
        explicit PreparedPush(MediaLineageAllocationScope allocation)
            : m_allocation(std::move(allocation)) {}
        PreparedPush(PreparedPush&&) noexcept = default;
        PreparedPush& operator=(PreparedPush&& other) noexcept;
        PreparedPush(const PreparedPush&) = delete;
        PreparedPush& operator=(const PreparedPush&) = delete;

        std::size_t size() const noexcept;
        ::media::Status replace(
            std::span<const MediaBufferRef> buffers);

    private:
        friend class MediaBlockingQueueStorage;
        MediaLineageAllocationScope m_allocation;
        MediaFixedCapacityStorage<MediaBufferRef> m_buffers;
    };

    MediaBlockingQueueStorage(const MediaQueuePolicy& policy,
                              PreparationAllocator preparationAllocator);

    static void allocatePreparedStorage(
        MediaFixedCapacityStorage<MediaBufferRef>& buffers,
        std::size_t capacity, std::pmr::memory_resource& resource);

    bool valid() const noexcept;
    bool supportsPreparedPush() const noexcept;
    bool empty() const noexcept;
    std::size_t size() const noexcept;
    MediaBufferRef& front() noexcept;
    const MediaBufferRef& at(std::size_t offset) const noexcept;

    void pushBack(MediaBufferRef buffer);
    void popFront() noexcept;
    void erase(std::size_t offset) noexcept;
    void clear() noexcept;

    ::media::Result<PreparedPush> prepare(
        std::span<const MediaBufferRef> buffers, MediaLineageAllocationScope allocation) const;
    void publish(PreparedPush& prepared) noexcept;

private:
    std::size_t ringIndex(std::size_t offset) const noexcept;
    void pushBackPrepared(MediaBufferRef buffer) noexcept;

    MediaQueueStorageMode m_mode = MediaQueueStorageMode::Unknown;
    PreparationAllocator m_preparationAllocator = nullptr;
    std::deque<MediaBufferRef> m_deque;
    MediaFixedCapacityStorage<MediaBufferRef> m_ring;
    std::size_t m_ringHead = 0;
    std::size_t m_ringSize = 0;
};

} // namespace media::ffmpeg::graph
