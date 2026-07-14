#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaVideoSyncController;

enum class MediaVideoSyncDecisionKind : std::uint8_t {
    NoAction = 0,
    Hold = 1,
    Display = 2,
    DisplayLate = 3,
    DisplayPreservedKeyFrame = 4,
    Drop = 5,
    RepeatPreviousFrame = 6,
    Reacquire = 7,
    DropOldGeneration = 8
};

class MediaVideoSyncDecision final {
public:
    MediaVideoSyncDecisionKind kind() const noexcept { return m_kind; }
    MediaRunningTime presentationOnMaster() const noexcept
    {
        return m_presentationOnMaster;
    }
    MediaRunningTime phaseError() const noexcept { return m_phaseError; }
    std::uint64_t generation() const noexcept { return m_generation; }
    std::uint64_t sequence() const noexcept { return m_sequence; }
    int consecutiveRecoveryActions() const noexcept
    {
        return m_consecutiveRecoveryActions;
    }

private:
    friend class MediaVideoSyncController;

    MediaVideoSyncDecision(MediaVideoSyncDecisionKind kind,
                           MediaRunningTime presentationOnMaster,
                           MediaRunningTime phaseError,
                           std::uint64_t generation,
                           std::uint64_t sequence,
                           int consecutiveRecoveryActions) noexcept
        : m_kind(kind)
        , m_presentationOnMaster(presentationOnMaster)
        , m_phaseError(phaseError)
        , m_generation(generation)
        , m_sequence(sequence)
        , m_consecutiveRecoveryActions(consecutiveRecoveryActions)
    {
    }

    MediaVideoSyncDecisionKind m_kind = MediaVideoSyncDecisionKind::NoAction;
    MediaRunningTime m_presentationOnMaster = MediaRunningTime::fromNanoseconds(0);
    MediaRunningTime m_phaseError = MediaRunningTime::fromNanoseconds(0);
    std::uint64_t m_generation = 0;
    std::uint64_t m_sequence = 0;
    int m_consecutiveRecoveryActions = 0;
};

} // namespace media::ffmpeg::graph
