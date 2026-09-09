#pragma once
#include <atomic>
namespace media::ffmpeg::graph {
class MediaGraphWorker;
class MediaGraphWorkerExitToken final {
public:
    bool exited() const noexcept { return m_exited.load(std::memory_order_acquire); }
    bool completed() const noexcept {
        return exited() || m_cancelledBeforeStart.load(std::memory_order_acquire);
    }
private:
    friend class MediaGraphWorker;
    friend class MediaGraphExecutionContext;
    std::atomic_bool m_exited{false};
    std::atomic_bool m_cancelledBeforeStart{false};
};
} // namespace media::ffmpeg::graph
