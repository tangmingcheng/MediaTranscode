#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/sync/MediaAvGenerationTransition.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvEpochTransitionService;

class MediaAvGenerationTransitionCoordinator final {
public:
    static ::media::Result<MediaAvGenerationTransitionCoordinator> create(
        MediaAvGenerationTransitionPlan plan);

    ::media::Result<MediaAvGenerationPurge> begin(
        std::uint64_t oldGeneration,
        std::uint64_t nextGeneration);
    ::media::Result<bool> acknowledge(
        MediaAvGenerationAcknowledgement acknowledgement);
    ::media::Status checkTimeout(MediaRunningTime elapsedSinceBegin);
    void abort() noexcept;
    bool outputPermitted(std::uint64_t generation) const noexcept;
    bool poisoned() const noexcept { return m_poisoned; }

private:
    friend class MediaAvEpochTransitionService;

    struct ActiveTransition final {
        MediaAvGenerationPurge purge;
        std::vector<bool> acknowledged;
    };

    struct CompletedTransition final {
        std::uint64_t sequence;
        std::uint64_t nextGeneration;
    };

    explicit MediaAvGenerationTransitionCoordinator(
        MediaAvGenerationTransitionPlan plan);
    ::media::Status permitInitial(std::uint64_t generation);
    ::media::Status publishCompletedGeneration(
        std::uint64_t completedTransitionSequence,
        std::uint64_t expectedNextGeneration);
    std::optional<std::size_t> participantIndex(
        MediaAvGenerationParticipant participant) const noexcept;
    ::media::Status poison(::media::ErrorInfo error);

    MediaAvGenerationTransitionPlan m_plan;
    std::optional<std::uint64_t> m_currentGeneration;
    std::optional<ActiveTransition> m_active;
    std::optional<CompletedTransition> m_completed;
    std::uint64_t m_nextSequence = 1;
    bool m_permitted = false;
    bool m_poisoned = false;
};

} // namespace media::ffmpeg::graph
