#include "internal/graph/runtime/context/MediaGraphPayloadCreditWakeupHub.h"
#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

namespace media::ffmpeg::graph {

void MediaGraphPayloadCreditWakeupHub::add(
    std::weak_ptr<MediaNodeWakeup> wakeup)
{
    std::lock_guard lock(m_mutex);
    if (!m_interrupted) m_wakeups.push_back(std::move(wakeup));
}

void MediaGraphPayloadCreditWakeupHub::onGraphPayloadCreditReleased() noexcept
{
    std::lock_guard lock(m_mutex);
    if (m_interrupted) return;
    for (auto it = m_wakeups.begin(); it != m_wakeups.end();) {
        if (auto wakeup = it->lock()) {
            wakeup->notify();
            ++it;
        } else {
            it = m_wakeups.erase(it);
        }
    }
}

void MediaGraphPayloadCreditWakeupHub::interrupt() noexcept
{
    std::lock_guard lock(m_mutex);
    m_interrupted = true;
    for (const auto& weak : m_wakeups) {
        if (auto wakeup = weak.lock()) wakeup->interrupt();
    }
    m_wakeups.clear();
}

} // namespace media::ffmpeg::graph
