#pragma once

#include "internal/graph/sync/MediaAudioPlaybackOrigin.h"
#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

class MediaAvReacquisitionCoordinator;
class MediaPlaybackEpochActivationCapability;
struct MediaAvEpochTransitionServiceTestAccess;

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

    ::media::Result<MediaAvGenerationPurge> beginReacquisition(
        std::uint64_t oldGeneration,
        std::uint64_t nextGeneration);
    ::media::Result<bool> acknowledge(
        MediaAvGenerationAcknowledgement acknowledgement);
    ::media::Status pollTransitionTimeout(MediaRunningTime elapsedSinceBegin);
    void abort() noexcept;
    MediaAvEpochTransitionSnapshot snapshot() const noexcept;
    const MediaAvGenerationTransitionPlan& transitionPlan() const noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;
    friend class MediaPlaybackEpochActivationCapability;
    friend struct MediaAvEpochTransitionServiceTestAccess;
    ::media::Status activateInitial(
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);
    ::media::Status activateNextAfter(
        std::uint64_t completedTransitionSequence,
        MediaPlaybackEpoch epoch,
        MediaAudioPlaybackOrigin audioOrigin);
    explicit MediaAvEpochTransitionService(
        MediaAvGenerationTransitionCoordinator coordinator);
    static ::media::Status validateEpochPair(
        const MediaPlaybackEpoch& epoch,
        const MediaAudioPlaybackOrigin& audioOrigin);
    ::media::Status failReacquisition(::media::ErrorInfo error);
    ::media::Status failLocked(::media::ErrorInfo error);

    mutable std::mutex m_mutex;
    MediaAvGenerationTransitionCoordinator m_coordinator;
    MediaAvGenerationReadiness m_readiness =
        MediaAvGenerationReadiness::Acquiring;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAudioPlaybackOrigin> m_audioOrigin;
    std::optional<::media::ErrorInfo> m_firstError;
};

} // namespace media::ffmpeg::graph
