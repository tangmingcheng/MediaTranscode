#pragma once
#if defined(__linux__)
#include <optional>

namespace media::ffmpeg::graph {
struct MediaNodeWaitObservation;

// Calling-thread scope only. Explicit restore reports failures to the wait;
// the noexcept destructor is a final restoration attempt during unwinding.
class MediaLinuxTimerSlackScope final {
public:
    enum class Operation { Read, Set, Restore };
    struct Failure final { Operation operation; int code; };
    explicit MediaLinuxTimerSlackScope(MediaNodeWaitObservation* observation) noexcept;
    ~MediaLinuxTimerSlackScope() noexcept;
    MediaLinuxTimerSlackScope(const MediaLinuxTimerSlackScope&) = delete;
    MediaLinuxTimerSlackScope& operator=(const MediaLinuxTimerSlackScope&) = delete;
    std::optional<Failure> enter() noexcept;
    std::optional<Failure> restore() noexcept;
private:
    MediaNodeWaitObservation* m_observation;
    unsigned long m_previous = 0;
    bool m_active = false;
};
} // namespace media::ffmpeg::graph
#endif
