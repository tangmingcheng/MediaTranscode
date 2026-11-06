#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyView.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanView.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
// Fixed source products stay alive and unmoved throughout candidate evaluation.
// All mutable fields are allocation-free quantities; no codec or identity is copied.
struct MediaRealtimeAvSourceCapacityState final {
    const MediaRealtimeAvSourcePlanningProducts& fixed;
    MediaAvSyncStartupCapacity startup;
    MediaAvSyncAudioServoPolicy audioServo;
    MediaRealtimeAvSyncAssemblyView assembly;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    std::optional<MediaAudioCorrectionReachabilityPlan> audioCorrection;
    explicit MediaRealtimeAvSourceCapacityState(const MediaRealtimeAvSourcePlanningProducts& source)
        : fixed(source), startup(static_cast<const MediaAvSyncStartupCapacity&>(
              MediaAvSyncStartupPolicyView(source.synchronization.startup))),
          audioServo(source.synchronization.audioServo), assembly(source.assembly),
          queues(source.queues), edgePolicies(source.edgePolicies), audioCorrection(source.audioCorrection) {}
    MediaRealtimeAvSourceCapacityState(MediaRealtimeAvSourcePlanningProducts&&) = delete;
    MediaRealtimeAvSourceCapacityState(const MediaRealtimeAvSourcePlanningProducts&&) = delete;
    // Call only after successful candidate planning. No identities or other fixed
    // products are changed; publishing to another source is an error.
    ::media::Status publish(MediaRealtimeAvSourcePlanningProducts& target) const;
};
} // namespace media::ffmpeg::graph
