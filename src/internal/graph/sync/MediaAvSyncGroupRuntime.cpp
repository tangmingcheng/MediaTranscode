#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

namespace media::ffmpeg::graph {
namespace {

bool reacquisitionPending(
    const std::optional<MediaPlaybackEpoch>& epoch,
    const std::optional<MediaAvReacquisitionRequest>& request) noexcept
{
    if (!request) return false;
    if (!epoch) return true;
    return request->reason == MediaAvReacquisitionReason::FutureGeneration
        ? epoch->generation < request->observedGeneration
        : epoch->generation <= request->observedGeneration;
}

} // namespace

MediaAvSyncGroupRuntime::MediaAvSyncGroupRuntime(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
    std::shared_ptr<MediaAvEpochTransitionService> transitionService)
    : m_key(std::move(key))
    , m_plan(std::move(plan))
    , m_clock(std::move(clock))
    , m_sharedNtpEpoch(std::move(sharedNtpEpoch))
    , m_transitionService(std::move(transitionService))
{
}

::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>
MediaAvSyncGroupRuntime::create(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch,
    std::shared_ptr<MediaAvEpochTransitionService> transitionService)
{
    if (!transitionService) {
        return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group requires an epoch transition service"));
    }
    if (!key.valid() || !clock) {
        return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group requires a key and steady master clock"));
    }
    auto status = MediaAvSyncPlanValidator::validate(plan);
    if (!status) {
        return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::failure(
            status.error());
    }
    const bool requiresNtp = *plan.topology ==
        MediaAvSyncTopology::SeparateRtpToSeparateRtp;
    if (static_cast<bool>(sharedNtpEpoch) != requiresNtp) {
        return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group clock bundle does not match its topology"));
    }
    return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::success(
        std::shared_ptr<MediaAvSyncGroupRuntime>(new MediaAvSyncGroupRuntime(
            std::move(key), std::move(plan), std::move(clock),
            std::move(sharedNtpEpoch), std::move(transitionService))));
}

::media::Result<MediaAvSyncGroupRuntime::GenerationDisposition>
MediaAvSyncGroupRuntime::observeGeneration(std::uint64_t generation)
{
    const auto transition = m_transitionService->snapshot();
    if (!transition.playbackEpoch || generation == 0) {
        return ::media::Result<GenerationDisposition>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync group has no active generation"));
    }
    if (transition.poisoned) {
        return ::media::Result<GenerationDisposition>::failure(
            ::media::ErrorInfo::cancelled(
                "Aborted A/V sync group rejects generation observations"));
    }
    if (generation < transition.playbackEpoch->generation) {
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::Old);
    }
    if (generation > transition.playbackEpoch->generation) {
        const MediaAvReacquisitionRequest request{
            generation, MediaAvReacquisitionReason::FutureGeneration};
        auto* coordinator = reacquisitionCoordinator();
        if (!coordinator) {
            return ::media::Result<GenerationDisposition>::failure(
                ::media::ErrorInfo::notInitialized(
                    "A/V sync group requires its planned reacquisition coordinator"));
        }
        auto observed = coordinator->observe(request);
        if (!observed) {
            return ::media::Result<GenerationDisposition>::failure(
                observed.error());
        }
        {
            std::lock_guard<std::mutex> lock(m_epochMutex);
            if (!m_reacquisitionRequest ||
                generation > m_reacquisitionRequest->observedGeneration) {
                m_reacquisitionRequest = request;
            }
        }
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::ReacquisitionRequired);
    }
    const auto reacquisition = reacquisitionSnapshot();
    const bool pendingRequest =
        reacquisition.phase != MediaAvReacquisitionPhase::Inactive ||
        reacquisition.reason.has_value();
    if (transition.readiness != MediaAvGenerationReadiness::Locked ||
        pendingRequest) {
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::ReacquisitionRequired);
    }
    return ::media::Result<GenerationDisposition>::success(
        GenerationDisposition::Current);
}

::media::Status MediaAvSyncGroupRuntime::requestReacquisition(
    MediaAvReacquisitionRequest request) noexcept
{
    auto* coordinator = reacquisitionCoordinator();
    if (!coordinator) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync group requires its planned reacquisition coordinator"));
    }
    auto status = coordinator->request(request);
    if (!status) return status;
    const auto transition = m_transitionService->snapshot();
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (!m_reacquisitionRequest ||
        !reacquisitionPending(
            transition.playbackEpoch, m_reacquisitionRequest) ||
        request.observedGeneration >
            m_reacquisitionRequest->observedGeneration) {
        m_reacquisitionRequest = request;
    }
    return status;
}

::media::Status MediaAvSyncGroupRuntime::installReacquisitionCoordinator(
    std::shared_ptr<MediaAvReacquisitionCoordinator> coordinator)
{
    if (!coordinator) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group cannot install a null reacquisition coordinator"));
    }
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (m_reacquisitionCoordinator) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group installs its reacquisition coordinator exactly once"));
    }
    m_reacquisitionCoordinator = std::move(coordinator);
    return ::media::Status::success();
}

MediaAvReacquisitionSnapshot
MediaAvSyncGroupRuntime::reacquisitionSnapshot() const noexcept
{
    auto* coordinator = reacquisitionCoordinator();
    return coordinator
        ? coordinator->snapshot()
        : MediaAvReacquisitionSnapshot{
              MediaAvReacquisitionPhase::Inactive,
              std::nullopt,
              std::nullopt};
}

::media::Status
MediaAvSyncGroupRuntime::markReacquisitionReadyForActivation(
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    auto* coordinator = reacquisitionCoordinator();
    return coordinator
        ? coordinator->markReadyForActivation(
              generation, transitionSequence)
        : ::media::Status::failure(
              ::media::ErrorInfo::notInitialized(
                  "A/V sync group has no reacquisition coordinator"));
}

::media::Result<MediaAvReacquisitionActivationReservation>
MediaAvSyncGroupRuntime::reserveReacquisitionActivation(
    std::uint64_t generation,
    std::uint64_t transitionSequence)
{
    auto* coordinator = reacquisitionCoordinator();
    return coordinator
        ? coordinator->reserveActivation(generation, transitionSequence)
        : ::media::Result<
              MediaAvReacquisitionActivationReservation>::failure(
              ::media::ErrorInfo::notInitialized(
                  "A/V sync group has no reacquisition coordinator"));
}

::media::Result<MediaAvStartupReleaseDisposition>
MediaAvSyncGroupRuntime::classifyStartupRelease(
    MediaAvStartupReleaseKind kind,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence) const noexcept
{
    auto* coordinator = reacquisitionCoordinator();
    if (!coordinator) {
        return ::media::Result<
            MediaAvStartupReleaseDisposition>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync group has no planned reacquisition coordinator"));
    }
    return ::media::Result<
        MediaAvStartupReleaseDisposition>::success(
        coordinator->classifyRelease(
            kind, generation, transitionSequence));
}

::media::Result<MediaAvStartupReleasePublicationReservation>
MediaAvSyncGroupRuntime::reserveStartupReleasePublication(
    MediaAvStartupReleaseKind kind,
    std::uint64_t generation,
    std::optional<std::uint64_t> transitionSequence)
{
    std::shared_ptr<MediaAvReacquisitionCoordinator> coordinator;
    {
        std::lock_guard<std::mutex> groupLock(m_epochMutex);
        coordinator = m_reacquisitionCoordinator;
    }
    if (!coordinator) {
        return ::media::Result<
            MediaAvStartupReleasePublicationReservation>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync group has no planned reacquisition coordinator"));
    }
    return ::media::Result<
        MediaAvStartupReleasePublicationReservation>::success(
        coordinator->reserveReleasePublication(
            kind, generation, transitionSequence));
}

std::optional<MediaAvReacquisitionRequest>
MediaAvSyncGroupRuntime::reacquisitionRequest() const noexcept
{
    const auto transition = m_transitionService->snapshot();
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (m_reacquisitionRequest &&
        !reacquisitionPending(transition.playbackEpoch,
                              m_reacquisitionRequest)) {
        return std::nullopt;
    }
    return m_reacquisitionRequest;
}

void MediaAvSyncGroupRuntime::markAborted() noexcept
{
    auto* coordinator = reacquisitionCoordinator();
    if (coordinator) {
        coordinator->abort();
    } else {
        m_transitionService->abort();
    }
}

void MediaAvSyncGroupRuntime::shutdown() noexcept
{
    markAborted();
}

MediaAvSyncGroupRuntime::LifecycleState
MediaAvSyncGroupRuntime::lifecycleState() const noexcept
{
    const auto transition = m_transitionService->snapshot();
    if (transition.poisoned) return LifecycleState::Aborted;
    const auto reacquisition = reacquisitionSnapshot();
    const bool pendingRequest =
        reacquisition.phase != MediaAvReacquisitionPhase::Inactive ||
        reacquisition.reason.has_value();
    if (transition.readiness == MediaAvGenerationReadiness::Locked &&
        !pendingRequest) {
        return LifecycleState::Active;
    }
    if (!transition.playbackEpoch) return LifecycleState::AwaitingEpoch;
    return LifecycleState::ReacquisitionRequired;
}

::media::Result<MediaPlaybackEpoch> MediaAvSyncGroupRuntime::playbackEpoch() const
{
    auto snapshot = m_transitionService->snapshot();
    return snapshot.playbackEpoch
        ? ::media::Result<MediaPlaybackEpoch>::success(*snapshot.playbackEpoch)
        : ::media::Result<MediaPlaybackEpoch>::failure(
              ::media::ErrorInfo::notInitialized(
                  "A/V sync group has no active playback epoch"));
}

MediaAvEpochTransitionSnapshot
MediaAvSyncGroupRuntime::epochTransitionSnapshot() const noexcept
{
    return m_transitionService->snapshot();
}

::media::Result<MediaAvOutputPermitCommitReservation>
MediaAvSyncGroupRuntime::reserveOutputCommit(
    std::uint64_t generation) const
{
    return m_transitionService->reserveOutputCommit(generation);
}

::media::Result<MediaAvActivatedOutputPermitReservation>
MediaAvSyncGroupRuntime::reserveActivatedOutput() const
{
    return m_transitionService->reserveActivatedOutput();
}

MediaAvReacquisitionCoordinator*
MediaAvSyncGroupRuntime::reacquisitionCoordinator() const noexcept
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    return m_reacquisitionCoordinator.get();
}

::media::Status MediaAvSyncGroupRuntime::pollEpochReacquisitionTimeout()
{
    auto* coordinator = reacquisitionCoordinator();
    if (coordinator) return coordinator->pollTimeout();
    return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "A/V sync group has no reacquisition coordinator"));
}

::media::Result<MediaRunningTime> MediaAvSyncGroupRuntime::mapCanonicalToMaster(
    MediaRunningTime canonicalTime) const noexcept
{
    auto epoch = playbackEpoch();
    if (!epoch) return ::media::Result<MediaRunningTime>::failure(epoch.error());
    auto fromStart = canonicalTime.checkedSubtract(epoch.value().sourceStart);
    if (!fromStart) return fromStart;
    return epoch.value().masterRelease.checkedAdd(fromStart.value());
}

} // namespace media::ffmpeg::graph
