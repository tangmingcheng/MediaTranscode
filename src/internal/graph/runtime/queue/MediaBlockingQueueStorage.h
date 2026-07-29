#pragma once

#include "internal/graph/model/MediaQueuePolicy.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <deque>
#include <span>
#include <vector>

namespace media::ffmpeg::graph {

class MediaBlockingQueueStorage final {
public:
    using PreparationAllocator = void (*) (
        std::vector<MediaBufferRef>& buffers,
        std::size_t capacity);

    class PreparedPush final {
    public:
        PreparedPush() = default;
        PreparedPush(PreparedPush&&) noexcept = default;
        PreparedPush& operator=(PreparedPush&&) noexcept = default;
        PreparedPush(const PreparedPush&) = delete;
        PreparedPush& operator=(const PreparedPush&) = delete;

        std::size_t size() const noexcept;
        ::media::Status replace(
            std::span<const MediaBufferRef> buffers);

    private:
        friend class MediaBlockingQueueStorage;
        std::vector<MediaBufferRef> m_buffers;
    };

    MediaBlockingQueueStorage(const MediaQueuePolicy& policy,
                              PreparationAllocator preparationAllocator);

    static void reserveWithDefaultAllocator(
        std::vector<MediaBufferRef>& buffers,
        std::size_t capacity);

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
        std::span<const MediaBufferRef> buffers) const;
    void publish(PreparedPush& prepared) noexcept;

private:
    std::size_t ringIndex(std::size_t offset) const noexcept;
    void pushBackPrepared(MediaBufferRef buffer) noexcept;

    MediaQueueStorageMode m_mode = MediaQueueStorageMode::Unknown;
    PreparationAllocator m_preparationAllocator = nullptr;
    std::deque<MediaBufferRef> m_deque;
    std::vector<MediaBufferRef> m_ring;
    std::size_t m_ringHead = 0;
    std::size_t m_ringSize = 0;
};

} // namespace media::ffmpeg::graph
