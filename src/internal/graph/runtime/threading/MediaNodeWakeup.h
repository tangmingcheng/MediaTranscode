#pragma once

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaNodeWakeup final {
public:
    using Sequence = std::uint64_t;

    Sequence sequence() const noexcept;
    void notify() noexcept;
    enum class WaitOutcome {
        Notified,
        Deadline,
        Interrupted
    };
    WaitOutcome wait(Sequence observedSequence,
                     std::optional<std::chrono::nanoseconds> timeout = std::nullopt);
    void interrupt() noexcept;
    void reset() noexcept;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    Sequence m_sequence = 0;
    bool m_interrupted = false;
};

} // namespace media::ffmpeg::graph
