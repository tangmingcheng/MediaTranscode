#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace media::ffmpeg::graph {

enum class MediaRtpUdpTransportPhase {
    ReceiveProtected,
    StopLifetimeAcquired,
    AbortLifetimeAcquired,
    CloseReceiveWait,
    CloseReceiveAcquired,
    Count
};

class MediaRtpUdpTransportPhaseController final {
public:
    explicit MediaRtpUdpTransportPhaseController(int maximumWaitMs);

    MediaRtpUdpTransportPhaseController(
        const MediaRtpUdpTransportPhaseController&) = delete;
    MediaRtpUdpTransportPhaseController& operator=(
        const MediaRtpUdpTransportPhaseController&) = delete;

    void arm(MediaRtpUdpTransportPhase phase) noexcept;
    bool waitUntilReached(MediaRtpUdpTransportPhase phase,
                          int timeoutMs) noexcept;
    void release(MediaRtpUdpTransportPhase phase) noexcept;
    bool healthy() const noexcept;

private:
    friend class MediaRtpUdpTransport;

    struct PhaseState final {
        bool armed = false;
        bool reached = false;
        bool released = false;
    };

    static std::size_t index(MediaRtpUdpTransportPhase phase) noexcept;
    void synchronize(MediaRtpUdpTransportPhase phase) noexcept;

    std::chrono::milliseconds m_maximumWait;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_healthy{true};
    std::array<PhaseState,
               static_cast<std::size_t>(MediaRtpUdpTransportPhase::Count)> m_phases{};
};

} // namespace media::ffmpeg::graph
