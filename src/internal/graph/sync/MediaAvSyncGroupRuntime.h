#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"
#include "internal/graph/sync/MediaAvReacquisitionRequest.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/time/MediaMasterClock.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"
#include "internal/graph/time/MediaSteadyMasterClock.h"

#include <memory>
#include <mutex>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaAvReacquisitionCoordinatorTestAccess;

class MediaAvSyncGroupRuntime final {
public:
    enum class GenerationDisposition { Old, Current, ReacquisitionRequired };
    enum class LifecycleState { AwaitingEpoch, Active, ReacquisitionRequired, Aborted };
    static ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>> create(
        MediaAvSyncGroupKey key,
        MediaAvSyncPlan plan,
        std::shared_ptr<MediaMasterClock> clock,
        std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
        std::shared_ptr<MediaAvEpochTransitionService> transitionService);

    const MediaAvSyncGroupKey& key() const noexcept { return m_key; }
    const MediaAvSyncPlan& plan() const noexcept { return m_plan; }
    const std::shared_ptr<MediaMasterClock>& clock() const noexcept { return m_clock; }
    const std::shared_ptr<const MediaSharedNtpEpoch>& sharedNtpEpoch() const noexcept
    {
        return m_sharedNtpEpoch;
    }
    ::media::Result<MediaPlaybackEpoch> playbackEpoch() const;
    MediaAvEpochTransitionSnapshot epochTransitionSnapshot() const noexcept;
    ::media::Status pollEpochReacquisitionTimeout();
    ::media::Result<GenerationDisposition> observeGeneration(
        std::uint64_t generation);
    ::media::Status requestReacquisition(MediaAvReacquisitionRequest request) noexcept;
    ::media::Status installReacquisitionCoordinator(
        std::shared_ptr<MediaAvReacquisitionCoordinator> coordinator);
    MediaAvReacquisitionSnapshot reacquisitionSnapshot() const noexcept;
    ::media::Status markReacquisitionReadyForActivation(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Result<MediaAvReacquisitionActivationReservation>
    reserveReacquisitionActivation(
        std::uint64_t generation,
        std::uint64_t transitionSequence);
    ::media::Result<MediaAvStartupReleaseDisposition> classifyStartupRelease(
        MediaAvStartupReleaseKind kind,
        std::uint64_t generation,
        std::optional<std::uint64_t> transitionSequence) const noexcept;
    ::media::Result<MediaAvStartupReleasePublicationReservation>
    reserveStartupReleasePublication(
        MediaAvStartupReleaseKind kind,
        std::uint64_t generation,
        std::optional<std::uint64_t> transitionSequence);
    std::optional<MediaAvReacquisitionRequest> reacquisitionRequest() const noexcept;
    void markAborted() noexcept;
    void shutdown() noexcept;
    LifecycleState lifecycleState() const noexcept;
    ::media::Result<MediaRunningTime> mapCanonicalToMaster(
        MediaRunningTime canonicalTime) const noexcept;

private:
    friend struct MediaAvReacquisitionCoordinatorTestAccess;

    MediaAvSyncGroupRuntime(MediaAvSyncGroupKey key,
                            MediaAvSyncPlan plan,
                            std::shared_ptr<MediaMasterClock> clock,
                            std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
                            std::shared_ptr<MediaAvEpochTransitionService> transitionService);
    MediaAvReacquisitionCoordinator* reacquisitionCoordinator() const noexcept;
    MediaAvSyncGroupKey m_key;
    MediaAvSyncPlan m_plan;
    std::shared_ptr<MediaMasterClock> m_clock;
    std::shared_ptr<const MediaSharedNtpEpoch> m_sharedNtpEpoch;
    std::shared_ptr<MediaAvEpochTransitionService> m_transitionService;
    mutable std::mutex m_epochMutex;
    std::shared_ptr<MediaAvReacquisitionCoordinator>
        m_reacquisitionCoordinator;
    std::optional<MediaAvReacquisitionRequest> m_reacquisitionRequest;
};

} // namespace media::ffmpeg::graph
