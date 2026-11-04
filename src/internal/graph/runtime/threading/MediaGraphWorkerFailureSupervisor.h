#pragma once

#include <atomic>
#include <mutex>

namespace media::ffmpeg::graph {

class MediaGraphWorkerFailureSupervisor final {
public:
    // Borrowed identities remain valid through join/disarm. Copying this exact
    // descriptor during failure notification never allocates or calls user code.
    struct CoordinatedStop final {
        void* owner = nullptr;
        void* context = nullptr;
        void (*invoke)(void*, void*) noexcept = nullptr;
    };

    void arm(CoordinatedStop coordinatedStop);
    void notifyPrimaryFailure();
    void disarm() noexcept;

private:
    std::atomic_bool m_notified{ false };
    std::mutex m_mutex;
    CoordinatedStop m_coordinatedStop;
};

} // namespace media::ffmpeg::graph
