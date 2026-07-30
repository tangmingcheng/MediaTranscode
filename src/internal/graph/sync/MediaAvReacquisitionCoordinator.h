#pragma once

#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvReacquisitionRequest.h"
#include "internal/graph/sync/MediaAvStartupReleaseKind.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvReacquisitionCoordinator;
class MediaAvSyncGroupRuntime;
struct MediaAvReacquisitionCoordinatorTestAccess;

class MediaAvStartupReleasePublicationReservation final {
public:
    MediaAvStartupReleasePublicationReservation(
        MediaAvStartupReleasePublicationReservation&& other) noexcept;
    MediaAvStartupReleasePublicationReservation& operator=(
        MediaAvStartupReleasePublicationReservation&& other) noexcept;
    MediaAvStartupReleasePublicationReservation(
        const MediaAvStartupReleasePublicationReservation&) = delete;
    MediaAvStartupReleasePublicationReservation& operator=(
        const MediaAvStartupReleasePublicationReservation&) = delete;
    ~MediaAvStartupReleasePublicationReservation() = default;

    MediaAvStartupReleaseDisposition disposition() const noexcept;
    void completePublished() noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;
    friend class MediaAvSyncGroupRuntime;

    MediaAvStartupReleasePublicationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        MediaAvStartupReleaseDisposition disposition,
        std::unique_lock<std::mutex> publicationLock) noexcept;

    std::shared_ptr<MediaAvReacquisitionCoordinator> m_owner;
    MediaAvStartupReleaseDisposition m_disposition =
        MediaAvStartupReleaseDisposition::Reject;
    std::unique_lock<std::mutex> m_publicationLock;
};

class MediaAvReacquisitionActivationReservation final {
public:
    MediaAvReacquisitionActivationReservation(
        MediaAvReacquisitionActivationReservation&& other) noexcept;
    MediaAvReacquisitionActivationReservation& operator=(
        MediaAvReacquisitionActivationReservation&& other) noexcept;
    MediaAvReacquisitionActivationReservation(
        const MediaAvReacquisitionActivationReservation&) = delete;
    MediaAvReacquisitionActivationReservation& operator=(
        const MediaAvReacquisitionActivationReservation&) = delete;
    ~MediaAvReacquisitionActivationReservation();

    ::media::Status authorizePublication();
    ::media::Status finalizePublication();
    void completePublished() noexcept;
    void abandon() noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;

    MediaAvReacquisitionActivationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        std::uint64_t generation,
        std::uint64_t transitionSequence,
        std::unique_lock<std::mutex> activationLock) noexcept;

    std::shared_ptr<MediaAvReacquisitionCoordinator> m_owner;
    std::uint64_t m_generation = 0;
    std::uint64_t m_transitionSequence = 0;
    std::unique_lock<std::mutex> m_activationLock;
    bool m_authorized = false;
    bool m_finalized = false;
    bool m_completed = false;
};

enum class MediaAvReacquisitionPhase : std::uint8_t {
    Inactive = 0,
    Purging = 1,
    Acquiring = 2,
    ReadyForActivation = 3,
    Aborted = 4,
    Activating = 5,
    Publishing = 6
};

struct MediaAvReacquisitionSnapshot final {
    MediaAvReacquisitionPhase phase;
    std::optional<MediaAvGenerationPurge> transition;
    std::optional<MediaAvReacquisitionReason> reason;
};

class MediaAvGenerationPublicationReservation final {
public:
    MediaAvGenerationPublicationReservation(
        MediaAvGenerationPublicationReservation&&) noexcept = default;
    MediaAvGenerationPublicationReservation& operator=(
        MediaAvGenerationPublicationReservation&&) noexcept = default;
    MediaAvGenerationPublicationReservation(
        const MediaAvGenerationPublicationReservation&) = delete;
    MediaAvGenerationPublicationReservation& operator=(
        const MediaAvGenerationPublicationReservation&) = delete;
    ~MediaAvGenerationPublicationReservation() = default;

private:
    friend class MediaAvGenerationArbitrationReservation;

    MediaAvGenerationPublicationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        std::unique_lock<std::mutex> activationLock) noexcept;

    std::shared_ptr<MediaAvReacquisitionCoordinator> m_owner;
    std::unique_lock<std::mutex> m_activationLock;
};

class MediaAvGenerationArbitrationReservation final {
public:
    MediaAvGenerationArbitrationReservation(
        MediaAvGenerationArbitrationReservation&&) noexcept = default;
    MediaAvGenerationArbitrationReservation& operator=(
        MediaAvGenerationArbitrationReservation&&) noexcept = default;
    MediaAvGenerationArbitrationReservation(
        const MediaAvGenerationArbitrationReservation&) = delete;
    MediaAvGenerationArbitrationReservation& operator=(
        const MediaAvGenerationArbitrationReservation&) = delete;
    ~MediaAvGenerationArbitrationReservation() = default;

    const MediaAvReacquisitionSnapshot& reacquisition() const noexcept
    {
        return m_reacquisition;
    }
    const MediaAvEpochTransitionSnapshot& epoch() const noexcept
    {
        return m_epoch;
    }
    MediaAvGenerationPublicationReservation
    retainPublicationAuthority() && noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;

    MediaAvGenerationArbitrationReservation(
        std::shared_ptr<MediaAvReacquisitionCoordinator> owner,
        MediaAvReacquisitionSnapshot reacquisition,
        MediaAvEpochTransitionSnapshot epoch,
        std::unique_lock<std::mutex> activationLock) noexcept;

    std::shared_ptr<MediaAvReacquisitionCoordinator> m_owner;
    MediaAvReacquisitionSnapshot m_reacquisition;
    MediaAvEpochTransitionSnapshot m_epoch;
    std::unique_lock<std::mutex> m_activationLock;
};

class MediaAvReacquisitionCoordinator final
    : public std::enable_shared_from_this<MediaAvReacquisitionCoordinator> {
public:
    static ::media::Result<std::shared_ptr<MediaAvReacquisitionCoordinator>>
    create(std::shared_ptr<MediaAvEpochTransitionService> transition,
           std::shared_ptr<MediaMasterClock> clock,
           std::vector<MediaAvGenerationParticipantGroup> participants);

    ::media::Status observe(MediaAvReacquisitionRequest request);
    ::media::Status request(MediaAvReacquisitionRequest request);
    ::media::Status pollTimeout();
    MediaAvReacquisitionSnapshot snapshot() const noexcept;
    MediaAvGenerationArbitrationReservation
    reserveGenerationArbitration();
    MediaAvStartupReleaseDisposition classifyRelease(
        MediaAvStartupReleaseKind kind,
        std::uint64_t generation,
        std::optional<std::uint64_t> transitionSequence) const noexcept;
    MediaAvStartupReleasePublicationReservation reserveReleasePublication(
        MediaAvStartupReleaseKind kind,
        std::uint64_t generation,
        std::optional<std::uint64_t> transitionSequence);
    ::media::Status markReadyForActivation(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Result<MediaAvReacquisitionActivationReservation>
    reserveActivation(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    void abort() noexcept;

private:
    friend class MediaAvReacquisitionActivationReservation;
    friend struct MediaAvReacquisitionCoordinatorTestAccess;

    MediaAvReacquisitionCoordinator(
        std::shared_ptr<MediaAvEpochTransitionService> transition,
        std::shared_ptr<MediaMasterClock> clock,
        std::vector<MediaAvGenerationParticipantGroup> participants);

    ::media::Status failTerminalLocked(::media::ErrorInfo error);
    ::media::Status validateAndQueueRequest(
        MediaAvReacquisitionRequest request);
    ::media::Status rejectIncompatibleEvidence(
        ::media::ErrorInfo error);
    std::unique_lock<std::mutex> acquireActivationArbitration();
    bool matchesActiveRequest(
        const MediaAvReacquisitionRequest& request) const noexcept;
    bool matchesTransition(
        std::uint64_t generation,
        std::uint64_t transitionSequence) const noexcept;
    MediaAvStartupReleaseDisposition classifyReleaseLocked(
        MediaAvStartupReleaseKind kind,
        std::uint64_t generation,
        std::optional<std::uint64_t> transitionSequence) const noexcept;
    ::media::Status authorizePublication(
        MediaAvReacquisitionActivationReservation& reservation);
    ::media::Status finalizePublication(
        MediaAvReacquisitionActivationReservation& reservation);
    void releasePublished(
        MediaAvReacquisitionActivationReservation& reservation) noexcept;
    void abandon(
        MediaAvReacquisitionActivationReservation& reservation) noexcept;

    mutable std::mutex m_activationMutex;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_activationWaitChanged;
    std::size_t m_activationWaiters = 0;
    std::shared_ptr<MediaAvEpochTransitionService> m_transitionService;
    std::shared_ptr<MediaMasterClock> m_clock;
    std::vector<MediaAvGenerationParticipantGroup> m_participants;
    MediaAvReacquisitionPhase m_phase =
        MediaAvReacquisitionPhase::Inactive;
    std::optional<MediaAvReacquisitionRequest> m_request;
    std::optional<MediaAvGenerationPurge> m_transition;
    std::optional<std::uint64_t> m_inFlightTransitionSequence;
    std::optional<std::uint64_t> m_lastPublishedTransitionSequence;
    std::optional<MediaRunningTime> m_beganAt;
    std::optional<::media::ErrorInfo> m_firstError;
};

} // namespace media::ffmpeg::graph
