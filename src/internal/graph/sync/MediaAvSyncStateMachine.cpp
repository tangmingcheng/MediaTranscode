#include "internal/graph/sync/MediaAvSyncStateMachine.h"

namespace media::ffmpeg::graph {

MediaAvSyncStateMachine::MediaAvSyncStateMachine(MediaAvSyncTopology topology) noexcept
    : m_topology(topology)
{
}

MediaAvSyncStatus MediaAvSyncStateMachine::transition(MediaAvSyncEvent event,
                                                       std::uint64_t generation)
{
    if (event == MediaAvSyncEvent::BeginAcquisition) {
        if (m_state != MediaAvSyncState::Idle || m_generation) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_generation = generation;
        m_state = MediaAvSyncState::AcquiringClock;
        return MediaAvSyncStatus::success();
    }
    if (event == MediaAvSyncEvent::RequireReacquisition) {
        if (!m_generation || generation < *m_generation ||
            m_state == MediaAvSyncState::Failed || m_state == MediaAvSyncState::Stopped ||
            m_state == MediaAvSyncState::Aborted) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_generation = generation;
        m_state = MediaAvSyncState::AcquiringClock;
        return MediaAvSyncStatus::success();
    }
    if (!m_generation || generation != *m_generation) {
        return MediaAvSyncStatus::failure(invalidTransition(event, generation));
    }

    switch (event) {
    case MediaAvSyncEvent::ClocksLocked:
        if (m_state != MediaAvSyncState::AcquiringClock) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_state = MediaAvSyncState::PrimingStreams;
        break;
    case MediaAvSyncEvent::StreamsPrimed:
        if (m_state != MediaAvSyncState::PrimingStreams) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_state = MediaAvSyncState::Armed;
        break;
    case MediaAvSyncEvent::Release:
        if (m_state != MediaAvSyncState::Armed) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_state = MediaAvSyncState::Released;
        break;
    case MediaAvSyncEvent::Run:
        if (m_state != MediaAvSyncState::Released) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_state = MediaAvSyncState::Running;
        break;
    case MediaAvSyncEvent::Fail:
        if (m_state == MediaAvSyncState::Failed || m_state == MediaAvSyncState::Stopped ||
            m_state == MediaAvSyncState::Aborted) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_state = MediaAvSyncState::Failed;
        break;
    case MediaAvSyncEvent::Stop:
        if (m_state == MediaAvSyncState::Aborted) {
            return MediaAvSyncStatus::failure(invalidTransition(event, generation));
        }
        m_state = MediaAvSyncState::Stopped;
        break;
    case MediaAvSyncEvent::Abort:
        m_state = MediaAvSyncState::Aborted;
        break;
    case MediaAvSyncEvent::BeginAcquisition:
    case MediaAvSyncEvent::RequireReacquisition:
        return MediaAvSyncStatus::failure(invalidTransition(event, generation));
    }
    return MediaAvSyncStatus::success();
}

MediaAvSyncError MediaAvSyncStateMachine::invalidTransition(
    MediaAvSyncEvent event,
    std::uint64_t generation) const
{
    const auto zero = MediaRunningTime::fromNanoseconds(0);
    return MediaAvSyncError(
        MediaAvSyncErrorCode::StartupInvalidTransition,
        m_topology,
        MediaAvSyncErrorState::Startup,
        "state_transition_" + std::to_string(static_cast<int>(event)),
        {}, {}, m_generation, generation, std::nullopt,
        zero, zero, 0, 0,
        "MediaAvSyncStateMachine rejects an invalid state transition");
}

void MediaAvSyncStateMachine::reset() noexcept
{
    m_state = MediaAvSyncState::Idle;
    m_generation.reset();
}

MediaAvSyncState MediaAvSyncStateMachine::state() const noexcept { return m_state; }
const std::optional<std::uint64_t>& MediaAvSyncStateMachine::generation() const noexcept
{
    return m_generation;
}

} // namespace media::ffmpeg::graph
