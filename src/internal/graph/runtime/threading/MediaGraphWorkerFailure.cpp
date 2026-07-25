#include "internal/graph/runtime/threading/MediaGraphWorkerFailure.h"

#include <utility>

namespace media::ffmpeg::graph {

bool MediaGraphWorkerFailureRecorder::recordFirst(MediaGraphWorkerFailure failure)
{
    std::lock_guard lock(m_mutex);
    if (m_primaryFailure) {
        return false;
    }
    m_primaryFailure = std::move(failure);
    m_hasFailure.store(true, std::memory_order_release);
    return true;
}

bool MediaGraphWorkerFailureRecorder::hasFailure() const noexcept
{
    return m_hasFailure.load(std::memory_order_acquire);
}

std::optional<MediaGraphWorkerFailure>
MediaGraphWorkerFailureRecorder::primaryFailure() const
{
    std::lock_guard lock(m_mutex);
    return m_primaryFailure;
}

void MediaGraphWorkerFailureRecorder::clear()
{
    std::lock_guard lock(m_mutex);
    m_primaryFailure.reset();
    m_hasFailure.store(false, std::memory_order_release);
}

} // namespace media::ffmpeg::graph
