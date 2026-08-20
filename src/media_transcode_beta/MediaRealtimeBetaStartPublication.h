#pragma once

#include <atomic>
#include <cstdint>

namespace media::beta {

class MediaRealtimeBetaStartPublication final {
public:
    MediaRealtimeBetaStartPublication() = default;
    MediaRealtimeBetaStartPublication(
        const MediaRealtimeBetaStartPublication&) = delete;
    MediaRealtimeBetaStartPublication& operator=(
        const MediaRealtimeBetaStartPublication&) = delete;

    void publishOwnership() noexcept
    {
        transition(State::Published);
    }

    void cancel() noexcept
    {
        transition(State::Cancelled);
    }

    bool waitForOwnership() const noexcept
    {
        State state = m_state.load(std::memory_order_acquire);
        while (state == State::Pending) {
            m_state.wait(State::Pending, std::memory_order_acquire);
            state = m_state.load(std::memory_order_acquire);
        }
        return state == State::Published;
    }

private:
    enum class State : std::uint8_t {
        Pending,
        Published,
        Cancelled
    };

    void transition(State destination) noexcept
    {
        State expected = State::Pending;
        if (m_state.compare_exchange_strong(
                expected, destination, std::memory_order_release,
                std::memory_order_acquire)) {
            m_state.notify_all();
        }
    }

    mutable std::atomic<State> m_state{State::Pending};
};

} // namespace media::beta
