#include "internal/graph/protocol/rtp/MediaRtpUdpTransportPhaseController.h"

#include <stdexcept>

namespace media::ffmpeg::graph {

MediaRtpUdpTransportPhaseController::MediaRtpUdpTransportPhaseController(
    int maximumWaitMs)
    : m_maximumWait(maximumWaitMs)
{
    if (maximumWaitMs <= 0) {
        throw std::invalid_argument(
            "RTP UDP transport phase controller requires positive maximum wait");
    }
}

void MediaRtpUdpTransportPhaseController::arm(
    MediaRtpUdpTransportPhase phase) noexcept
{
    try {
        std::lock_guard lock(m_mutex);
        auto& state = m_phases.at(index(phase));
        state.armed = true;
        state.reached = false;
        state.released = false;
    } catch (...) {
        m_healthy.store(false, std::memory_order_release);
    }
}

bool MediaRtpUdpTransportPhaseController::waitUntilReached(
    MediaRtpUdpTransportPhase phase,
    int timeoutMs) noexcept
{
    if (timeoutMs <= 0) return false;
    try {
        std::unique_lock lock(m_mutex);
        return m_condition.wait_for(
            lock,
            std::chrono::milliseconds(timeoutMs),
            [&] { return m_phases.at(index(phase)).reached; });
    } catch (...) {
        m_healthy.store(false, std::memory_order_release);
        return false;
    }
}

void MediaRtpUdpTransportPhaseController::release(
    MediaRtpUdpTransportPhase phase) noexcept
{
    try {
        std::lock_guard lock(m_mutex);
        m_phases.at(index(phase)).released = true;
        m_condition.notify_all();
    } catch (...) {
        m_healthy.store(false, std::memory_order_release);
    }
}

bool MediaRtpUdpTransportPhaseController::healthy() const noexcept
{
    return m_healthy.load(std::memory_order_acquire);
}

std::size_t MediaRtpUdpTransportPhaseController::index(
    MediaRtpUdpTransportPhase phase) noexcept
{
    return static_cast<std::size_t>(phase);
}

void MediaRtpUdpTransportPhaseController::synchronize(
    MediaRtpUdpTransportPhase phase) noexcept
{
    try {
        std::unique_lock lock(m_mutex);
        auto& state = m_phases.at(index(phase));
        state.reached = true;
        m_condition.notify_all();
        if (state.armed && !state.released) {
            const bool released = m_condition.wait_for(
                lock,
                m_maximumWait,
                [&] { return state.released; });
            if (!released) {
                m_healthy.store(false, std::memory_order_release);
            }
        }
    } catch (...) {
        m_healthy.store(false, std::memory_order_release);
    }
}

} // namespace media::ffmpeg::graph
