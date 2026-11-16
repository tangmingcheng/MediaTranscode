#pragma once

#include "internal/graph/sync/MediaPlaybackActivation.h"
#include "internal/graph/sync/MediaAvGenerationTransitionCoordinator.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/sync/MediaAvSourceDrain.h"

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
    MediaPlaybackActivation activation;
    std::optional<std::uint64_t> completedTransitionSequence;
    MediaAvOutputPermitCommitReservation reservation;
};

struct MediaAvEpochTransitionSnapshot final {
    MediaAvGenerationReadiness readiness;
    std::optional<MediaPlaybackActivation> activation;
    const MediaPlaybackEpoch* playbackEpoch() const noexcept
    { return activation ? &activation->epoch() : nullptr; }
    bool outputPermitted;
    bool poisoned;
    std::optional<std::uint64_t> completedTransitionSequence;
};

class MediaAvEpochTransitionService final {
public:
    static ::media::Result<std::shared_ptr<MediaAvEpochTransitionService>> create(
        MediaAvGenerationTransitionPlan plan, MediaTranscodeStreamSet members);
    static ::media::Result<std::shared_ptr<MediaAvEpochTransitionService>> createInitialOnly(
        MediaTranscodeStreamSet members);
    MediaTranscodeStreamSet members() const noexcept { return m_members; }

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
    ::media::Status beginSourceDrain(MediaAvSourceDrainRequest request);
    ::media::Status sealSourceDrainInput(std::uint64_t generation, MediaStreamKind stream,
        std::uint64_t ingressGeneration, std::uint64_t acceptedFrontier);
    ::media::Result<bool> acknowledgeSourceDrained(std::uint64_t generation, MediaStreamKind stream);
    std::optional<MediaAvSourceDrainSnapshot> sourceDrainSnapshot() const noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;
    friend class MediaPlaybackEpochActivationCapability;
    friend class MediaOutputEpochActivationCapability;
    friend struct MediaAvEpochTransitionServiceTestAccess;
    ::media::Status activateInitial(
        MediaPlaybackActivation activation);
    ::media::Status activateNextAfter(
        std::uint64_t completedTransitionSequence,
        MediaPlaybackActivation activation);
    explicit MediaAvEpochTransitionService(
        std::optional<MediaAvGenerationTransitionCoordinator> coordinator,
        MediaTranscodeStreamSet members);
    bool outputPermittedLocked(std::uint64_t generation) const noexcept;
    ::media::Status failReacquisition(::media::ErrorInfo error);
    ::media::Status failLocked(::media::ErrorInfo error);

    mutable std::mutex m_mutex;
    std::optional<MediaAvGenerationTransitionCoordinator> m_coordinator;
    bool m_aborted = false;
    MediaAvGenerationReadiness m_readiness =
        MediaAvGenerationReadiness::Acquiring;
    const MediaTranscodeStreamSet m_members;
    std::optional<MediaPlaybackActivation> m_activation;
    std::optional<std::uint64_t> m_completedTransitionSequence;
    std::optional<::media::ErrorInfo> m_firstError;
    std::optional<MediaAvSourceDrainSnapshot> m_sourceDrain;
};

} // namespace media::ffmpeg::graph
