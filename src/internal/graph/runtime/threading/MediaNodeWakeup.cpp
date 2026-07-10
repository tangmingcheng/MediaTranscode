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

bool MediaNodeWakeup::waitForChange(Sequence observedSequence)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_condition.wait(lock, [&] {
        return m_interrupted || m_sequence != observedSequence;
    });
    return !m_interrupted;
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
