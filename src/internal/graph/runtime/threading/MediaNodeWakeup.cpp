#include "internal/graph/runtime/threading/MediaNodeWakeup.h"

namespace media::ffmpeg::graph {

MediaNodeWakeup::Sequence MediaNodeWakeup::sequence() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sequence;
}

void MediaNodeWakeup::notify() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_sequence;
    }
    m_condition.notify_one();
}

MediaNodeWakeup::WaitOutcome MediaNodeWakeup::wait(
    Sequence observedSequence,
    std::optional<std::chrono::nanoseconds> timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto changedPredicate = [&] {
        return m_interrupted || m_sequence != observedSequence;
    };
    bool changed = true;
    if (timeout) {
        changed = m_condition.wait_for(lock, *timeout, changedPredicate);
    } else {
        m_condition.wait(lock, changedPredicate);
    }
    if (m_interrupted) return WaitOutcome::Interrupted;
    return changed ? WaitOutcome::Notified : WaitOutcome::Deadline;
}

void MediaNodeWakeup::interrupt() noexcept
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interrupted = true;
        ++m_sequence;
    }
    m_condition.notify_all();
}

void MediaNodeWakeup::reset() noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_interrupted = false;
}

} // namespace media::ffmpeg::graph
