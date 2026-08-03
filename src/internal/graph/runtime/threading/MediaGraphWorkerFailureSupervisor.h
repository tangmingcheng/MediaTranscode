#pragma once

#include <atomic>
#include <functional>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaGraphWorkerFailureSupervisor final {
public:
    using CoordinatedStop = std::function<void()>;

    void arm(CoordinatedStop coordinatedStop);
    void notifyPrimaryFailure();
    bool coordinating() const noexcept;
    void disarm() noexcept;

private:
    std::atomic_bool m_notified{ false };
    std::mutex m_mutex;
    CoordinatedStop m_coordinatedStop;
};

} // namespace media::ffmpeg::graph
