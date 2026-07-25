#pragma once

#include "internal/graph/sync/MediaAvSyncError.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaAvSyncState {
    Idle,
    AcquiringClock,
    PrimingStreams,
    Armed,
    Released,
    Running,
    Failed,
    Stopped,
    Aborted
};

enum class MediaAvSyncEvent {
    BeginAcquisition,
    ClocksLocked,
    StreamsPrimed,
    Release,
    Run,
    RequireReacquisition,
    Fail,
    Stop,
    Abort
};

class MediaAvSyncStateMachine final {
public:
    explicit MediaAvSyncStateMachine(MediaAvSyncTopology topology) noexcept;

    MediaAvSyncStatus transition(MediaAvSyncEvent event, std::uint64_t generation);
    void reset() noexcept;

    MediaAvSyncState state() const noexcept;
    const std::optional<std::uint64_t>& generation() const noexcept;

private:
    MediaAvSyncError invalidTransition(MediaAvSyncEvent event,
                                       std::uint64_t generation) const;

    MediaAvSyncTopology m_topology;
    MediaAvSyncState m_state = MediaAvSyncState::Idle;
    std::optional<std::uint64_t> m_generation;
};

} // namespace media::ffmpeg::graph
