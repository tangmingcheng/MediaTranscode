#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

#include <limits>

namespace media::ffmpeg::graph {

MediaAvSyncGroupRuntime::MediaAvSyncGroupRuntime(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch)
    : m_key(std::move(key))
    , m_plan(std::move(plan))
    , m_clock(std::move(clock))
    , m_sharedNtpEpoch(std::move(sharedNtpEpoch))
{
}

::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>
MediaAvSyncGroupRuntime::create(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaMasterClock> clock)
{
    if (!key.valid() || !clock) {
        return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V sync group requires a key and master clock"));
    }
    auto status = MediaAvSyncPlanValidator::validate(plan);
    if (!status) {
        return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::failure(
            status.error());
    }
    return ::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>::success(
        std::shared_ptr<MediaAvSyncGroupRuntime>(new MediaAvSyncGroupRuntime(
            std::move(key), std::move(plan), std::move(clock), nullptr)));
}

::media::Result<std::shared_ptr<MediaAvSyncGroupRuntime>>
MediaAvSyncGroupRuntime::create(
    MediaAvSyncGroupKey key,
    MediaAvSyncPlan plan,
    std::shared_ptr<MediaSteadyMasterClock> clock,
    std::shared_ptr<const MediaSharedNtpEpoch> sharedNtpEpoch)
{
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
            std::move(sharedNtpEpoch))));
}

::media::Status MediaAvSyncGroupRuntime::activatePlaybackEpoch(
    MediaPlaybackEpoch epoch)
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (epoch.generation == 0 || m_epoch ||
        m_lifecycleState != LifecycleState::AwaitingEpoch) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch activation requires an inactive group and positive generation"));
    }
    m_epoch = epoch;
    m_reacquisitionRequest.reset();
    m_lifecycleState = LifecycleState::Active;
    return ::media::Status::success();
}

::media::Status MediaAvSyncGroupRuntime::activateNextPlaybackEpoch(
    MediaPlaybackEpoch epoch)
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    std::uint64_t expectedGeneration = 0;
    if (m_reacquisitionRequest && m_epoch) {
        if (m_reacquisitionRequest->reason ==
            MediaAvReacquisitionReason::FutureGeneration) {
            expectedGeneration = m_reacquisitionRequest->observedGeneration;
        } else if (m_epoch->generation <
                   std::numeric_limits<std::uint64_t>::max()) {
            expectedGeneration = m_epoch->generation + 1;
        }
    }
    if (epoch.generation == 0 || !m_epoch ||
        m_lifecycleState != LifecycleState::ReacquisitionRequired ||
        !m_reacquisitionRequest ||
        epoch.generation != expectedGeneration ||
        epoch.generation <= m_epoch->generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Playback epoch reset requires a strictly newer generation"));
    }
    m_epoch = epoch;
    m_reacquisitionRequest.reset();
    m_lifecycleState = LifecycleState::Active;
    return ::media::Status::success();
}

::media::Result<MediaAvSyncGroupRuntime::GenerationDisposition>
MediaAvSyncGroupRuntime::observeGeneration(std::uint64_t generation)
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (!m_epoch || generation == 0) {
        return ::media::Result<GenerationDisposition>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync group has no active generation"));
    }
    if (m_lifecycleState == LifecycleState::Aborted) {
        return ::media::Result<GenerationDisposition>::failure(
            ::media::ErrorInfo::cancelled(
                "Aborted A/V sync group rejects generation observations"));
    }
    if (generation < m_epoch->generation) {
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::Old);
    }
    if (generation > m_epoch->generation) {
        m_lifecycleState = LifecycleState::ReacquisitionRequired;
        const MediaAvReacquisitionRequest request{
            generation, MediaAvReacquisitionReason::FutureGeneration};
        if (!m_reacquisitionRequest ||
            generation > m_reacquisitionRequest->observedGeneration) {
            m_reacquisitionRequest = request;
        }
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::ReacquisitionRequired);
    }
    if (m_lifecycleState != LifecycleState::Active) {
        return ::media::Result<GenerationDisposition>::success(
            GenerationDisposition::ReacquisitionRequired);
    }
    return ::media::Result<GenerationDisposition>::success(
        GenerationDisposition::Current);
}

::media::Status MediaAvSyncGroupRuntime::requestReacquisition(
    MediaAvReacquisitionRequest request) noexcept
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (!m_epoch || request.observedGeneration == 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "Reacquisition request requires an active playback epoch"));
    }
    if (m_lifecycleState == LifecycleState::Aborted) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "Aborted A/V sync group rejects reacquisition requests"));
    }
    const bool future = request.reason ==
        MediaAvReacquisitionReason::FutureGeneration;
    if (!future &&
        m_epoch->generation == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Playback generation is exhausted"));
    }
    const bool generationValid = future
        ? request.observedGeneration > m_epoch->generation
        : request.observedGeneration == m_epoch->generation;
    if (!generationValid) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Reacquisition reason does not match the observed generation"));
    }
    m_lifecycleState = LifecycleState::ReacquisitionRequired;
    // The first cause for a generation is authoritative; only evidence of a
    // strictly newer generation supersedes the observable request.
    if (!m_reacquisitionRequest ||
        request.observedGeneration >
            m_reacquisitionRequest->observedGeneration) {
        m_reacquisitionRequest = request;
    }
    return ::media::Status::success();
}

std::optional<MediaAvReacquisitionRequest>
MediaAvSyncGroupRuntime::reacquisitionRequest() const noexcept
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    return m_reacquisitionRequest;
}

void MediaAvSyncGroupRuntime::markAborted() noexcept
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    m_lifecycleState = LifecycleState::Aborted;
}

void MediaAvSyncGroupRuntime::shutdown() noexcept
{
    markAborted();
}

MediaAvSyncGroupRuntime::LifecycleState
MediaAvSyncGroupRuntime::lifecycleState() const noexcept
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    return m_lifecycleState;
}

::media::Result<MediaPlaybackEpoch> MediaAvSyncGroupRuntime::playbackEpoch() const
{
    std::lock_guard<std::mutex> lock(m_epochMutex);
    if (!m_epoch) {
        return ::media::Result<MediaPlaybackEpoch>::failure(
            ::media::ErrorInfo::notInitialized(
                "A/V sync group playback epoch is not active"));
    }
    return ::media::Result<MediaPlaybackEpoch>::success(*m_epoch);
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
