#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/runtime/threading/MediaNodeDeadlineWakePolicy.h"

#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaNodeWakeup final {
public:
    using Sequence = std::uint64_t;

    MediaNodeWakeup() noexcept;
    ~MediaNodeWakeup();

    MediaNodeWakeup(const MediaNodeWakeup&) = delete;
    MediaNodeWakeup& operator=(const MediaNodeWakeup&) = delete;

    Sequence sequence() const noexcept;
    void notify() noexcept;
    enum class WaitOutcome {
        Notified,
        Deadline,
        Interrupted
    };
    ::media::Result<WaitOutcome> wait(
        Sequence observedSequence,
        MediaNodeDeadlineWakePolicy wakePolicy,
        std::optional<std::chrono::nanoseconds> timeout = std::nullopt);
    void interrupt() noexcept;
    void reset() noexcept;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    Sequence m_sequence = 0;
    bool m_interrupted = false;
    std::optional<MediaNodeDeadlineWakePolicy> m_activeWaitPolicy;
#if defined(_WIN32)
    void* m_notificationEvent = nullptr;
    void* m_deadlineTimer = nullptr;
    unsigned long m_platformError = 0;
#endif
};

} // namespace media::ffmpeg::graph
