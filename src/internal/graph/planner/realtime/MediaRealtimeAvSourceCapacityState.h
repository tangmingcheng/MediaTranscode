#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceFixedProducts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyView.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanView.h"
#include "media_transcode/Result.h"
#include <utility>

namespace media::ffmpeg::graph {
// Fixed source products stay alive and unmoved throughout candidate evaluation.
// All mutable fields are allocation-free quantities; no codec or identity is copied.
struct MediaRealtimeAvSourceCapacityState final {
private:
    friend class MediaRealtimeAvSourceRuntimePlanner;
    const MediaRealtimeAvSourceFixedView fixed;
    MediaAvSyncStartupCapacity startup;
    MediaAvSyncAudioServoPolicy audioServo;
    MediaRealtimeAvSyncAssemblyView assembly;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    std::optional<MediaAudioCorrectionReachabilityPlan> audioCorrection;
    bool m_valid;
public:
    MediaRealtimeAvSourceCapacityState(const MediaRealtimeAvSourceCapacityState&) = delete;
    MediaRealtimeAvSourceCapacityState& operator=(const MediaRealtimeAvSourceCapacityState&) = delete;
    MediaRealtimeAvSourceCapacityState(MediaRealtimeAvSourceCapacityState&& other)
        : fixed(other.fixed), startup(other.startup), audioServo(other.audioServo), assembly(other.assembly),
          queues(other.queues), edgePolicies(other.edgePolicies), audioCorrection(other.audioCorrection),
          m_valid(std::exchange(other.m_valid, false)) {}
    MediaRealtimeAvSourceCapacityState& operator=(MediaRealtimeAvSourceCapacityState&&) = delete;
    explicit MediaRealtimeAvSourceCapacityState(const MediaRealtimeAvSourcePlanningProducts& source)
        : fixed(source), startup(static_cast<const MediaAvSyncStartupCapacity&>(
              MediaAvSyncStartupPolicyView(source.synchronization.startup))),
          audioServo(source.synchronization.audioServo), assembly(source.assembly),
          queues(source.queues), edgePolicies(source.edgePolicies), audioCorrection(source.audioCorrection), m_valid(true) {}
    MediaRealtimeAvSourceCapacityState(MediaRealtimeAvSourcePlanningProducts&&) = delete;
    MediaRealtimeAvSourceCapacityState(const MediaRealtimeAvSourcePlanningProducts&&) = delete;
    // Call only after successful candidate planning. No identities or other fixed
    // products are changed; publishing to another source is an error.
    ::media::Status publish(MediaRealtimeAvSourcePlanningProducts& target) const;
private:
    MediaRealtimeAvSourceCapacityState(const MediaRealtimeAvSourceFixedProducts& source,
        MediaAvSyncStartupCapacity quantities, MediaAvSyncAudioServoPolicy servo,
        MediaRealtimeAvSyncAssemblyView assemblyView, MediaGraphQueueParameters queueValues,
        MediaRealtimeEdgePolicySet edges, std::optional<MediaAudioCorrectionReachabilityPlan> correction)
        : fixed(source), startup(quantities), audioServo(servo), assembly(assemblyView),
          queues(queueValues), edgePolicies(edges), audioCorrection(correction), m_valid(true) {}
};
} // namespace media::ffmpeg::graph
