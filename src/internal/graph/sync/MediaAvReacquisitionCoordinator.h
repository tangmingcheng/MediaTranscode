#pragma once

#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvGenerationParticipantGroup.h"
#include "internal/graph/sync/MediaAvReacquisitionRequest.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaAvReacquisitionPhase : std::uint8_t {
    Inactive = 0,
    Purging = 1,
    Acquiring = 2,
    ReadyForActivation = 3,
    Aborted = 4
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
    ::media::Status markReadyForActivation(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Status markActivated(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    void abort() noexcept;

private:
    MediaAvReacquisitionCoordinator(
        std::shared_ptr<MediaAvEpochTransitionService> transition,
        std::shared_ptr<MediaMasterClock> clock,
        std::vector<MediaAvGenerationParticipantGroup> participants);

    ::media::Status failTerminal(::media::ErrorInfo error);
    ::media::Status validateAndQueueRequest(
        MediaAvReacquisitionRequest request);
    bool matchesActiveRequest(
        const MediaAvReacquisitionRequest& request) const noexcept;
    bool matchesTransition(
        std::uint64_t generation,
        std::uint64_t transitionSequence) const noexcept;

    mutable std::mutex m_mutex;
    std::shared_ptr<MediaAvEpochTransitionService> m_transitionService;
    std::shared_ptr<MediaMasterClock> m_clock;
    std::vector<MediaAvGenerationParticipantGroup> m_participants;
    MediaAvReacquisitionPhase m_phase =
        MediaAvReacquisitionPhase::Inactive;
    std::optional<MediaAvReacquisitionRequest> m_request;
    std::optional<MediaAvGenerationPurge> m_transition;
    std::optional<MediaRunningTime> m_beganAt;
    std::optional<::media::ErrorInfo> m_firstError;
};

} // namespace media::ffmpeg::graph
