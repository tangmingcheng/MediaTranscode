#include "internal/graph/sync/MediaAvEpochTransitionService.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaAvOutputPermitCommitReservation::
    MediaAvOutputPermitCommitReservation(
        std::unique_lock<std::mutex> lock) noexcept
    : m_lock(std::move(lock))
{
}

// Activation entry points are intentionally private to the runtime capability.

MediaAvEpochTransitionService::MediaAvEpochTransitionService(
    MediaAvGenerationTransitionCoordinator coordinator)
    : m_coordinator(std::move(coordinator))
{
}

::media::Result<std::shared_ptr<MediaAvEpochTransitionService>>
MediaAvEpochTransitionService::create(MediaAvGenerationTransitionPlan plan)
{
    auto coordinator = MediaAvGenerationTransitionCoordinator::create(
        std::move(plan));
    if (!coordinator) {
        return ::media::Result<std::shared_ptr<MediaAvEpochTransitionService>>::failure(
            coordinator.error());
    }
    return ::media::Result<std::shared_ptr<MediaAvEpochTransitionService>>::success(
        std::shared_ptr<MediaAvEpochTransitionService>(
            new MediaAvEpochTransitionService(
                std::move(coordinator).value())));
}

::media::Status MediaAvEpochTransitionService::validateEpochPair(
    const MediaPlaybackEpoch& epoch,
    const MediaAudioPlaybackOrigin& audioOrigin)
{
    if (epoch.generation == 0 || audioOrigin.generation != epoch.generation ||
        audioOrigin.sourceStart != epoch.sourceStart ||
        audioOrigin.masterRelease != epoch.masterRelease ||
        audioOrigin.epochOutputSampleIndex < 0 ||
        audioOrigin.outputSampleRate <= 0) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Playback epoch and audio origin must describe the same valid generation"));
    }
    return ::media::Status::success();
}

::media::Status MediaAvEpochTransitionService::activateInitial(
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin)
{
    auto valid = validateEpochPair(epoch, audioOrigin);
    if (!valid) return valid;
    std::lock_guard lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_epoch || m_audioOrigin ||
        m_readiness != MediaAvGenerationReadiness::Acquiring) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Initial epoch activation requires an acquiring empty service"));
    }
    auto permitted = m_coordinator.permitInitial(epoch.generation);
    if (!permitted) return permitted;
    m_epoch = epoch;
    m_audioOrigin = audioOrigin;
    m_completedTransitionSequence.reset();
    m_readiness = MediaAvGenerationReadiness::Locked;
    return ::media::Status::success();
}

::media::Result<MediaAvGenerationPurge>
MediaAvEpochTransitionService::beginReacquisition(
    std::uint64_t oldGeneration,
    std::uint64_t nextGeneration)
{
    std::lock_guard lock(m_mutex);
    if (m_firstError) {
        return ::media::Result<MediaAvGenerationPurge>::failure(
            *m_firstError);
    }
    if (m_readiness != MediaAvGenerationReadiness::Locked) {
        return ::media::Result<MediaAvGenerationPurge>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Reacquisition requires a locked generation"));
    }
    auto purge = m_coordinator.begin(oldGeneration, nextGeneration);
    if (!purge) return purge;
    m_readiness = MediaAvGenerationReadiness::Reacquire;
    return purge;
}

::media::Result<bool> MediaAvEpochTransitionService::acknowledge(
    MediaAvGenerationAcknowledgement acknowledgement)
{
    std::lock_guard lock(m_mutex);
    if (m_firstError) {
        return ::media::Result<bool>::failure(*m_firstError);
    }
    if (m_readiness != MediaAvGenerationReadiness::Reacquire) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Generation acknowledgement requires reacquisition"));
    }
    auto acknowledged = m_coordinator.acknowledge(
        std::move(acknowledgement));
    if (!acknowledged) {
        auto failed = failLocked(acknowledged.error());
        return ::media::Result<bool>::failure(failed.error());
    }
    if (acknowledged.value()) {
        m_readiness = MediaAvGenerationReadiness::Acquiring;
    }
    return acknowledged;
}

::media::Status MediaAvEpochTransitionService::pollTransitionTimeout(
    MediaRunningTime elapsedSinceBegin)
{
    std::lock_guard lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_readiness != MediaAvGenerationReadiness::Reacquire) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Generation timeout polling requires reacquisition"));
    }
    auto status = m_coordinator.checkTimeout(elapsedSinceBegin);
    if (!status) {
        return failLocked(status.error());
    }
    return status;
}

::media::Status MediaAvEpochTransitionService::activateNextAfter(
    std::uint64_t completedTransitionSequence,
    MediaPlaybackEpoch epoch,
    MediaAudioPlaybackOrigin audioOrigin)
{
    auto valid = validateEpochPair(epoch, audioOrigin);
    if (!valid) return failReacquisition(valid.error());
    std::lock_guard lock(m_mutex);
    if (m_firstError) {
        return ::media::Status::failure(*m_firstError);
    }
    if (m_readiness != MediaAvGenerationReadiness::Acquiring || !m_epoch ||
        !m_audioOrigin || epoch.generation <= m_epoch->generation) {
        return failLocked(::media::ErrorInfo::invalidArgument(
            "Next epoch activation requires a completed newer generation"));
    }
    auto published = m_coordinator.publishCompletedGeneration(
        completedTransitionSequence, epoch.generation);
    if (!published) return failLocked(published.error());
    m_epoch = epoch;
    m_audioOrigin = audioOrigin;
    m_completedTransitionSequence = completedTransitionSequence;
    m_readiness = MediaAvGenerationReadiness::Locked;
    return ::media::Status::success();
}

::media::Status MediaAvEpochTransitionService::failReacquisition(
    ::media::ErrorInfo error)
{
    std::lock_guard lock(m_mutex);
    return failLocked(std::move(error));
}

::media::Status MediaAvEpochTransitionService::failLocked(
    ::media::ErrorInfo error)
{
    if (!m_firstError) {
        m_firstError = std::move(error);
    }
    m_coordinator.abort();
    m_readiness = MediaAvGenerationReadiness::Reacquire;
    return ::media::Status::failure(*m_firstError);
}

void MediaAvEpochTransitionService::abort() noexcept
{
    std::lock_guard lock(m_mutex);
    m_coordinator.abort();
    m_readiness = MediaAvGenerationReadiness::Reacquire;
}

MediaAvEpochTransitionSnapshot
MediaAvEpochTransitionService::snapshot() const noexcept
{
    std::lock_guard lock(m_mutex);
    const std::uint64_t generation = m_epoch ? m_epoch->generation : 0;
    return MediaAvEpochTransitionSnapshot{
        m_readiness,
        m_epoch,
        m_audioOrigin,
        m_coordinator.outputPermitted(generation),
        m_coordinator.poisoned(),
        m_completedTransitionSequence};
}

::media::Result<MediaAvOutputPermitCommitReservation>
MediaAvEpochTransitionService::reserveOutputCommit(
    std::uint64_t generation) const
{
    std::unique_lock lock(m_mutex);
    if (m_firstError || m_readiness != MediaAvGenerationReadiness::Locked ||
        !m_epoch || m_epoch->generation != generation ||
        !m_coordinator.outputPermitted(generation)) {
        return ::media::Result<
            MediaAvOutputPermitCommitReservation>::failure(
                ::media::ErrorInfo::cancelled(
                    "A/V output commit requires the exact open generation permit"));
    }
    return ::media::Result<MediaAvOutputPermitCommitReservation>::success(
        MediaAvOutputPermitCommitReservation(std::move(lock)));
}

::media::Result<MediaAvActivatedOutputPermitReservation>
MediaAvEpochTransitionService::reserveActivatedOutput() const
{
    std::unique_lock lock(m_mutex);
    if (m_firstError || m_readiness != MediaAvGenerationReadiness::Locked ||
        !m_epoch || !m_audioOrigin ||
        !m_coordinator.outputPermitted(m_epoch->generation)) {
        return ::media::Result<
            MediaAvActivatedOutputPermitReservation>::failure(
                ::media::ErrorInfo::cancelled(
                    "A/V output activation requires the exact locked generation"));
    }
    return ::media::Result<
        MediaAvActivatedOutputPermitReservation>::success(
            MediaAvActivatedOutputPermitReservation{
                *m_epoch, *m_audioOrigin, m_completedTransitionSequence,
                MediaAvOutputPermitCommitReservation(std::move(lock))});
}

const MediaAvGenerationTransitionPlan&
MediaAvEpochTransitionService::transitionPlan() const noexcept
{
    return m_coordinator.m_plan;
}

} // namespace media::ffmpeg::graph
