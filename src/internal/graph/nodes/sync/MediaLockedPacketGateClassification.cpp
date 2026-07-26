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
    std::uint64_t generation)
{
    if (generation == 0) {
        return invalidClassification(
            "Locked packet gate requires a nonzero generation");
    }
    if (transitionActive(reacquisition.phase)) {
        if (!reacquisition.transition) {
            return invalidClassification(
                "Locked packet gate requires a complete active transition");
        }
        if (generation ==
            reacquisition.transition->oldGeneration) {
            return ::media::Result<
                MediaLockedPacketGateDisposition>::success(
                MediaLockedPacketGateDisposition::DropOldGeneration);
        }
        if (generation ==
            reacquisition.transition->nextGeneration) {
            return ::media::Result<
                MediaLockedPacketGateDisposition>::success(
                MediaLockedPacketGateDisposition::
                    WithholdForReacquisition);
        }
        return invalidClassification(
            "Locked packet gate rejects an unplanned transition generation");
    }
    if (reacquisition.phase != MediaAvReacquisitionPhase::Inactive ||
        reacquisition.transition) {
        return invalidClassification(
            "Locked packet gate rejects generation evidence outside a live group");
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
