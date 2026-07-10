#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaNodeWakeup final {
public:
    using Sequence = std::uint64_t;

    Sequence sequence() const noexcept;
    void notify() noexcept;
    bool waitForChange(Sequence observedSequence);
    void interrupt() noexcept;
    void reset() noexcept;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    Sequence m_sequence = 0;
    bool m_interrupted = false;
};

} // namespace media::ffmpeg::graph
