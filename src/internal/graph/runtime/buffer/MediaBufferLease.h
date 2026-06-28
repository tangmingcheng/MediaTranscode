#pragma once

#include "internal/graph/runtime/buffer/MediaBufferRef.h"

#include <functional>

namespace media::ffmpeg::graph {

class MediaBufferLease final {
public:
    using ReleaseCallback = std::function<void(MediaBufferRef)>;

    MediaBufferLease() = default;
    MediaBufferLease(MediaBufferRef buffer, ReleaseCallback releaseCallback = {});
    ~MediaBufferLease();

    MediaBufferLease(const MediaBufferLease&) = delete;
    MediaBufferLease& operator=(const MediaBufferLease&) = delete;

    MediaBufferLease(MediaBufferLease&& other) noexcept;
    MediaBufferLease& operator=(MediaBufferLease&& other) noexcept;

    MediaBufferRef get() const noexcept;
    MediaBufferRef detach() noexcept;
    void reset(MediaBufferRef buffer = {}, ReleaseCallback releaseCallback = {});

    explicit operator bool() const noexcept;
    MediaBuffer& operator*() const noexcept;
    MediaBuffer* operator->() const noexcept;

private:
    void release() noexcept;

private:
    MediaBufferRef m_buffer;
    ReleaseCallback m_releaseCallback;
};

} // namespace media::ffmpeg::graph
