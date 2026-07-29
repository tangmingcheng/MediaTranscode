#include "internal/graph/nodes/sync/MediaLockedPacketGateClassification.h"

#include "internal/graph/sync/MediaAvEpochTransitionService.h"
#include "internal/graph/sync/MediaAvReacquisitionCoordinator.h"

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaLockedPacketGateDisposition> invalidClassification(
    const char* message)
{
    return ::media::Result<MediaLockedPacketGateDisposition>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

bool transitionActive(MediaAvReacquisitionPhase phase) noexcept
{
    return phase == MediaAvReacquisitionPhase::Purging ||
        phase == MediaAvReacquisitionPhase::Acquiring ||
        phase == MediaAvReacquisitionPhase::ReadyForActivation;
}

} // namespace

::media::Result<MediaLockedPacketGateDisposition>
classifyLockedPacketGateGeneration(
    const MediaAvReacquisitionSnapshot& reacquisition,
    const MediaAvEpochTransitionSnapshot& epoch,
    std::uint64_t generation,
    std::uint64_t plannedInitialGeneration)
{
    if (generation == 0 || plannedInitialGeneration == 0) {
        return invalidClassification(
            "Locked packet gate requires exact nonzero generation authority");
    }
    if (transitionActive(reacquisition.phase)) {
        if (!reacquisition.transition) {
            return invalidClassification(
                "Locked packet gate requires a complete active transition");
        }
        const auto& transition = *reacquisition.transition;
        const auto expectedReadiness =
            reacquisition.phase == MediaAvReacquisitionPhase::Purging
            ? MediaAvGenerationReadiness::Reacquire
            : MediaAvGenerationReadiness::Acquiring;
        if (epoch.poisoned ||
            epoch.readiness != expectedReadiness ||
            !epoch.playbackEpoch ||
            epoch.playbackEpoch->generation != transition.oldGeneration ||
            !epoch.audioOrigin ||
            epoch.audioOrigin->generation != transition.oldGeneration ||
            epoch.outputPermitted) {
            return invalidClassification(
                "Locked packet gate requires a consistent live transition epoch");
        }
        if (generation ==
            transition.oldGeneration) {
            return ::media::Result<
                MediaLockedPacketGateDisposition>::success(
                MediaLockedPacketGateDisposition::DropOldGeneration);
        }
        if (generation ==
            transition.nextGeneration) {
            return ::media::Result<
                MediaLockedPacketGateDisposition>::success(
                reacquisition.phase ==
                        MediaAvReacquisitionPhase::Purging
                    ? MediaLockedPacketGateDisposition::
                          WithholdForReacquisition
                    : MediaLockedPacketGateDisposition::
                          PassToReacquisition);
        }
        return invalidClassification(
            "Locked packet gate rejects an unplanned transition generation");
    }
    if (reacquisition.phase != MediaAvReacquisitionPhase::Inactive ||
        reacquisition.transition) {
        return invalidClassification(
            "Locked packet gate rejects generation evidence outside a live group");
    }
    if (!epoch.poisoned &&
        epoch.readiness == MediaAvGenerationReadiness::Acquiring &&
        !epoch.playbackEpoch && !epoch.audioOrigin &&
        !epoch.outputPermitted &&
        !epoch.completedTransitionSequence) {
        return generation == plannedInitialGeneration
            ? ::media::Result<MediaLockedPacketGateDisposition>::success(
                  MediaLockedPacketGateDisposition::
                      PassToInitialAcquisition)
            : invalidClassification(
                  "Locked packet gate rejects generation outside the planned initial acquisition");
    }
    if (epoch.poisoned ||
        epoch.readiness != MediaAvGenerationReadiness::Locked ||
        !epoch.playbackEpoch ||
        !epoch.outputPermitted) {
        return invalidClassification(
            "Locked packet gate requires an active permitted playback epoch");
    }
    if (generation < epoch.playbackEpoch->generation) {
        return invalidClassification(
            "Locked packet gate rejects generation regression");
    }
    if (generation > epoch.playbackEpoch->generation) {
        return invalidClassification(
            "Locked packet gate rejects an unplanned future generation");
    }
    return ::media::Result<
        MediaLockedPacketGateDisposition>::success(
        MediaLockedPacketGateDisposition::Pass);
}

} // namespace media::ffmpeg::graph
