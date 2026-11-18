#include "internal/graph/planner/realtime/MediaRealtimeAvSourceCapacityState.h"

namespace media::ffmpeg::graph {
::media::Status MediaRealtimeAvSourceCapacityState::publish(MediaRealtimeAvSourcePlanningProducts& target) const
{
    if (!m_valid || !fixed.belongsTo(target) || target.assembly.audio.has_value() != assembly.audio.has_value() ||
        &target.assembly.inputClock != &assembly.inputClock ||
        &target.assembly.video.sourceIdentity != &assembly.video.sourceIdentity ||
        &target.assembly.video.timing != &assembly.video.timing ||
        (assembly.audio && (&target.assembly.audio->sourceIdentity != &assembly.audio->sourceIdentity ||
            &target.assembly.audio->duration != &assembly.audio->duration)))
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source capacity publication requires its original fixed owner and members"));
    auto& policy = target.synchronization.startup;
    static_cast<MediaAvSyncStartupCapacity&>(policy) = startup;
    target.synchronization.audioServo = audioServo;
    target.assembly.video.acquiringCapacity = assembly.video.acquiringCapacity;
    target.assembly.video.acquiringTimeout = assembly.video.acquiringTimeout;
    if (assembly.audio) {
        target.assembly.audio->acquiringCapacity = assembly.audio->acquiringCapacity;
        target.assembly.audio->acquiringTimeout = assembly.audio->acquiringTimeout;
    }
    target.queues = queues;
    target.edgePolicies = edgePolicies;
    target.audioCorrection = audioCorrection;
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
