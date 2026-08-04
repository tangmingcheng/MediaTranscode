#include "internal/graph/runtime/threading/MediaGraphWorkerFailureSupervisor.h"

#include <utility>

namespace media::ffmpeg::graph {

void MediaGraphWorkerFailureSupervisor::arm(CoordinatedStop coordinatedStop)
{
    std::lock_guard lock(m_mutex);
    m_coordinatedStop = std::move(coordinatedStop);
    m_notified.store(false, std::memory_order_release);
}

void MediaGraphWorkerFailureSupervisor::notifyPrimaryFailure()
{
    if (m_notified.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    CoordinatedStop coordinatedStop;
    {
        std::lock_guard lock(m_mutex);
        coordinatedStop = m_coordinatedStop;
    }
    if (coordinatedStop) {
        coordinatedStop();
    }
}

void MediaGraphWorkerFailureSupervisor::disarm() noexcept
{
    std::lock_guard lock(m_mutex);
    m_coordinatedStop = {};
}

} // namespace media::ffmpeg::graph
