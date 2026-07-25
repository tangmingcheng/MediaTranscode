#include "internal/graph/runtime/queue/MediaSpscQueue.h"

namespace media::ffmpeg::graph {

MediaSpscQueue::MediaSpscQueue(MediaQueuePolicy policy)
    : m_queue([&] {
          if (policy.mode == MediaQueueMode::Unknown || policy.mode == MediaQueueMode::Blocking) {
              policy.mode = MediaQueueMode::SpscRing;
          }
          return policy;
      }())
{
}

::media::Status MediaSpscQueue::push(MediaBufferRef buffer)
{
    return m_queue.push(std::move(buffer));
}

MediaQueuePushOutcome MediaSpscQueue::pushOutcome(MediaBufferRef buffer)
{
    return m_queue.pushOutcome(std::move(buffer));
}

::media::Status MediaSpscQueue::pop(MediaBufferRef& out)
{
    return m_queue.pop(out);
}

bool MediaSpscQueue::tryPop(MediaBufferRef& out)
{
    return m_queue.tryPop(out);
}

void MediaSpscQueue::close()
{
    m_queue.close();
}

void MediaSpscQueue::abort()
{
    m_queue.abort();
}

void MediaSpscQueue::clear()
{
    m_queue.clear();
}

bool MediaSpscQueue::closed() const
{
    return m_queue.closed();
}

bool MediaSpscQueue::aborted() const
{
    return m_queue.aborted();
}

std::size_t MediaSpscQueue::size() const
{
    return m_queue.size();
}

std::size_t MediaSpscQueue::capacity() const
{
    return m_queue.capacity();
}

const MediaQueuePolicy& MediaSpscQueue::policy() const noexcept
{
    return m_queue.policy();
}

const MediaQueueMetrics& MediaSpscQueue::metrics() const noexcept
{
    return m_queue.metrics();
}

} // namespace media::ffmpeg::graph
