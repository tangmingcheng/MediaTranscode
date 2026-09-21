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
class MediaOutputEpochActivationCapability;
struct MediaAvEpochTransitionServiceTestAccess;

class MediaAvOutputPermitCommitReservation final {
public:
    MediaAvOutputPermitCommitReservation(
        MediaAvOutputPermitCommitReservation&&) noexcept = default;
    MediaAvOutputPermitCommitReservation& operator=(
        MediaAvOutputPermitCommitReservation&&) noexcept = default;
    MediaAvOutputPermitCommitReservation(
        const MediaAvOutputPermitCommitReservation&) = delete;
    MediaAvOutputPermitCommitReservation& operator=(
        const MediaAvOutputPermitCommitReservation&) = delete;

private:
    friend class MediaAvEpochTransitionService;
    explicit MediaAvOutputPermitCommitReservation(
        std::unique_lock<std::mutex> lock) noexcept;

    std::unique_lock<std::mutex> m_lock;
};

struct MediaAvActivatedOutputPermitReservation final {
    MediaPlaybackEpoch epoch;
    MediaAudioPlaybackOrigin audioOrigin;
    std::optional<std::uint64_t> completedTransitionSequence;
    MediaAvOutputPermitCommitReservation reservation;
};

struct MediaAvEpochTransitionSnapshot final {
    MediaAvGenerationReadiness readiness;
    std::optional<MediaPlaybackEpoch> playbackEpoch;
    std::optional<MediaAudioPlaybackOrigin> audioOrigin;
    bool outputPermitted;
    bool poisoned;
    std::optional<std::uint64_t> completedTransitionSequence;
};

class MediaAvEpochTransitionService final {
public:
    static ::media::Result<std::shared_ptr<MediaAvEpochTransitionService>> create(
        MediaAvGenerationTransitionPlan plan);
    static std::shared_ptr<MediaAvEpochTransitionService> createInitialOnly();

    ::media::Result<MediaAvGenerationPurge> beginReacquisition(
        MediaAvTransitionOrigin origin,
        std::uint64_t nextGeneration);
    ::media::Result<bool> acknowledge(
        MediaAvGenerationAcknowledgement acknowledgement);
    ::media::Status pollTransitionTimeout(MediaRunningTime elapsedSinceBegin);
    void abort() noexcept;
    MediaAvEpochTransitionSnapshot snapshot() const noexcept;
    ::media::Result<MediaAvOutputPermitCommitReservation>
    reserveOutputCommit(std::uint64_t generation) const;
    ::media::Result<MediaAvActivatedOutputPermitReservation>
    reserveActivatedOutput() const;
    const MediaAvGenerationTransitionPlan* transitionPlan() const noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;
    friend class MediaPlaybackEpochActivationCapability;
    friend class MediaOutputEpochActivationCapability;
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
    MediaAvEpochTransitionService() = default;
    bool outputPermittedLocked(std::uint64_t generation) const noexcept;
    static ::media::Status validateEpochPair(
        const MediaPlaybackEpoch& epoch,
        const MediaAudioPlaybackOrigin& audioOrigin);
    ::media::Status failReacquisition(::media::ErrorInfo error);
    ::media::Status failLocked(::media::ErrorInfo error);

    mutable std::mutex m_mutex;
    std::optional<MediaAvGenerationTransitionCoordinator> m_coordinator;
    bool m_aborted = false;
    MediaAvGenerationReadiness m_readiness =
        MediaAvGenerationReadiness::Acquiring;
    std::optional<MediaPlaybackEpoch> m_epoch;
    std::optional<MediaAudioPlaybackOrigin> m_audioOrigin;
    std::optional<std::uint64_t> m_completedTransitionSequence;
    std::optional<::media::ErrorInfo> m_firstError;
};

} // namespace media::ffmpeg::graph
