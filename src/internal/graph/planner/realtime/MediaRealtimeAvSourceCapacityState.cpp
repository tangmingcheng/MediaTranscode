#include "internal/graph/planner/realtime/MediaRealtimeAvSourceCapacityState.h"

namespace media::ffmpeg::graph {
::media::Status MediaRealtimeAvSourceCapacityState::publish(MediaRealtimeAvSourcePlanningProducts& target) const
{
    if (&target != &fixed || target.assembly.audio.has_value() != assembly.audio.has_value())
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Source capacity publication requires its original fixed owner and members"));
    auto& policy = target.synchronization.startup;
    policy.videoCapacity = startup.videoCapacity;
    policy.audioCapacity = startup.audioCapacity;
    policy.videoByteCapacity = startup.videoByteCapacity;
    policy.audioByteCapacity = startup.audioByteCapacity;
    policy.maximumVideoUnitBytes = startup.maximumVideoUnitBytes;
    policy.maximumAudioUnitBytes = startup.maximumAudioUnitBytes;
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
