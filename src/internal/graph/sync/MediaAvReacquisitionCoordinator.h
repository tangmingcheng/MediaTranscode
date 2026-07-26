#pragma once

#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvReacquisitionRequest.h"
#include "internal/graph/sync/MediaAvStartupReleaseKind.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

class MediaAvReacquisitionCoordinator;
class MediaAvSyncGroupRuntime;

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
        MediaAvStartupReleaseDisposition disposition,
        std::unique_lock<std::mutex> publicationLock) noexcept;

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
    void completePublished() noexcept;
    void abandon() noexcept;

private:
    friend class MediaAvReacquisitionCoordinator;

    MediaAvReacquisitionActivationReservation(
        MediaAvReacquisitionCoordinator* owner,
        std::uint64_t generation,
        std::uint64_t transitionSequence,
        std::unique_lock<std::mutex> activationLock) noexcept;

    MediaAvReacquisitionCoordinator* m_owner = nullptr;
    std::uint64_t m_generation = 0;
    std::uint64_t m_transitionSequence = 0;
    std::unique_lock<std::mutex> m_activationLock;
    bool m_authorized = false;
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

class MediaAvReacquisitionCoordinator final {
public:
    static ::media::Result<std::unique_ptr<MediaAvReacquisitionCoordinator>>
    create(std::shared_ptr<MediaAvEpochTransitionService> transition,
           std::shared_ptr<MediaMasterClock> clock,
           std::vector<MediaAvGenerationParticipantGroup> participants);

    ::media::Status observe(MediaAvReacquisitionRequest request);
    ::media::Status request(MediaAvReacquisitionRequest request);
    ::media::Status pollTimeout();
    MediaAvReacquisitionSnapshot snapshot() const noexcept;
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

    MediaAvReacquisitionCoordinator(
        std::shared_ptr<MediaAvEpochTransitionService> transition,
        std::shared_ptr<MediaMasterClock> clock,
        std::vector<MediaAvGenerationParticipantGroup> participants);

    ::media::Status failTerminalLocked(::media::ErrorInfo error);
    ::media::Status validateAndQueueRequest(
        MediaAvReacquisitionRequest request);
    ::media::Status rejectIncompatibleEvidence(
        ::media::ErrorInfo error);
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
    void completePublished(
        MediaAvReacquisitionActivationReservation& reservation) noexcept;
    void abandon(
        MediaAvReacquisitionActivationReservation& reservation) noexcept;

    mutable std::mutex m_activationMutex;
    mutable std::mutex m_mutex;
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
