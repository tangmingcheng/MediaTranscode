#include "internal/graph/runtime/MediaGraphJitterBuffer.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

MediaGraphJitterBuffer::MediaGraphJitterBuffer(MediaLatencyPolicy policy)
    : m_policy(policy)
{
}

void MediaGraphJitterBuffer::setPolicy(MediaLatencyPolicy policy) noexcept
{
    m_policy = policy;
}

const MediaLatencyPolicy& MediaGraphJitterBuffer::policy() const noexcept
{
    return m_policy;
}

void MediaGraphJitterBuffer::push(MediaBufferRef buffer)
{
    if (!buffer) {
        return;
    }

    if (!m_policy.realtime()) {
        m_queue.push_back(std::move(buffer));
        return;
    }

    auto it = std::upper_bound(
        m_queue.begin(),
        m_queue.end(),
        buffer,
        [](const MediaBufferRef& lhs, const MediaBufferRef& rhs) {
            return lhs && rhs && lhs->pts() < rhs->pts();
        });

    m_queue.insert(it, std::move(buffer));
}

bool MediaGraphJitterBuffer::tryPop(MediaBufferRef& out)
{
    if (m_queue.empty()) {
        return false;
    }

    out = std::move(m_queue.front());
    m_queue.pop_front();
    return true;
}

void MediaGraphJitterBuffer::clear()
{
    m_queue.clear();
}

std::size_t MediaGraphJitterBuffer::size() const noexcept
{
    return m_queue.size();
}

bool MediaGraphJitterBuffer::empty() const noexcept
{
    return m_queue.empty();
}

} // namespace media::ffmpeg::graph
