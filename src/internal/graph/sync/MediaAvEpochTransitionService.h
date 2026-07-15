#pragma once

#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaAvEpochTransitionSnapshot final {
    MediaAvGenerationReadiness readiness;
    std::optional<MediaPlaybackEpoch> playbackEpoch;
    std::optional<MediaAudioPlaybackOrigin> audioOrigin;
    bool outputPermitted;
    bool poisoned;
};

class MediaAvEpochTransitionService final {
public:
    static ::media::Result<std::shared_ptr<MediaAvEpochTransitionService>> create(
        MediaAvGenerationTransitionPlan plan);

    ::media::Status activateInitial(
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);
    ::media::Result<MediaAvGenerationPurge> beginReacquisition(
        std::uint64_t oldGeneration,
        std::uint64_t nextGeneration);
    ::media::Result<bool> acknowledge(
        MediaAvGenerationAcknowledgement acknowledgement);
    ::media::Status pollTransitionTimeout(MediaRunningTime elapsedSinceBegin);
    ::media::Status activateNextAfter(
        std::uint64_t completedTransitionSequence,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);
    void abort() noexcept;
    MediaAvEpochTransitionSnapshot snapshot() const noexcept;

private:
    explicit MediaAvEpochTransitionService(
        MediaAvGenerationTransitionCoordinator coordinator);
    static ::media::Status validateEpochPair(
        const MediaPlaybackEpoch& epoch,
        const MediaAudioPlaybackOrigin& audioOrigin);

    mutable std::mutex m_mutex;
    MediaAvGenerationTransitionCoordinator m_coordinator;
    MediaAvGenerationReadiness m_readiness =
        MediaAvGenerationReadiness::Acquiring;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAudioPlaybackOrigin> m_audioOrigin;
};

} // namespace media::ffmpeg::graph
