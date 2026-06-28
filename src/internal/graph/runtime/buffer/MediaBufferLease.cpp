#include "internal/graph/runtime/buffer/MediaBufferLease.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaBufferLease::MediaBufferLease(MediaBufferRef buffer, ReleaseCallback releaseCallback)
    : m_buffer(std::move(buffer))
    , m_releaseCallback(std::move(releaseCallback))
{
}

MediaBufferLease::~MediaBufferLease()
{
    release();
}

MediaBufferLease::MediaBufferLease(MediaBufferLease&& other) noexcept
    : m_buffer(std::move(other.m_buffer))
    , m_releaseCallback(std::move(other.m_releaseCallback))
{
}

MediaBufferLease& MediaBufferLease::operator=(MediaBufferLease&& other) noexcept
{
    if (this != &other) {
        release();
        m_buffer = std::move(other.m_buffer);
        m_releaseCallback = std::move(other.m_releaseCallback);
    }
    return *this;
}

MediaBufferRef MediaBufferLease::get() const noexcept
{
    return m_buffer;
}

MediaBufferRef MediaBufferLease::detach() noexcept
{
    m_releaseCallback = {};
    return std::move(m_buffer);
}

void MediaBufferLease::reset(MediaBufferRef buffer, ReleaseCallback releaseCallback)
{
    release();
    m_buffer = std::move(buffer);
    m_releaseCallback = std::move(releaseCallback);
}

MediaBufferLease::operator bool() const noexcept
{
    return static_cast<bool>(m_buffer);
}

MediaBuffer& MediaBufferLease::operator*() const noexcept
{
    return *m_buffer;
}

MediaBuffer* MediaBufferLease::operator->() const noexcept
{
    return m_buffer.get();
}

void MediaBufferLease::release() noexcept
{
    if (!m_buffer) {
        return;
    }

    MediaBufferRef buffer = std::move(m_buffer);
    if (m_releaseCallback) {
        m_releaseCallback(std::move(buffer));
    }
}

} // namespace media::ffmpeg::graph
