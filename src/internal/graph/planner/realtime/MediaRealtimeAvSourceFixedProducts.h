#pragma once
#include "internal/graph/model/MediaRtpControlDelivery.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceClockPlanner.h"

namespace media::ffmpeg::graph {
// Owning declarations only. Candidate quantities are held separately.
struct MediaRealtimeAvSourceFixedProducts final {
    MediaAudioPipelinePlan audioPipeline;
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput;
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncFixedPlan synchronization;
    MediaRealtimeAvSourceClockFixedPlan clock;
    MediaThreadingPolicy threadingPolicy;
    MediaRunningTime activationOutputLead;
    bool videoFilterActive;
    MediaAvGenerationTransitionPlan transition;
    MediaRtpControlDelivery rtpControlDelivery;
};

// Mapping only: no policy selection, allocation, or owner lifetime extension.
class MediaRealtimeAvSourceFixedView final {
public:
    const MediaAudioPipelinePlan& audioPipeline;
    const std::optional<MediaRealtimeRtpInputNodePlan>& isolatedAudioInput;
    const MediaAvSyncGroupKey& groupKey;
    const MediaAvSyncFixedFields& synchronization;
    const MediaAvSyncAudioServoPolicy& audioServoPolicy;
    const MediaAvSyncStartupFixedPolicy& startup;
    const MediaThreadingPolicy& threadingPolicy;
    const MediaRunningTime activationOutputLead;
    const bool videoFilterActive;
    const MediaAvGenerationTransitionPlan& transition;
    const MediaRealtimeAvSourceTimingFacts& sourceTiming;
    const MediaRtpControlDelivery rtpControlDelivery;

    explicit MediaRealtimeAvSourceFixedView(const MediaRealtimeAvSourcePlanningProducts& source)
        : audioPipeline(source.audioPipeline), isolatedAudioInput(source.isolatedAudioInput), groupKey(source.groupKey),
          synchronization(source.synchronization), audioServoPolicy(source.audioServoPolicy),
          startup(source.synchronization.startup),
          threadingPolicy(source.threadingPolicy), activationOutputLead(source.activationOutputLead),
          videoFilterActive(source.videoFilterActive), transition(source.transition),
          sourceTiming(source.sourceTiming), rtpControlDelivery(source.rtpControlDelivery), m_completeOwner(&source), m_fixedOwner(nullptr) {}
    explicit MediaRealtimeAvSourceFixedView(const MediaRealtimeAvSourceFixedProducts& source)
        : audioPipeline(source.audioPipeline), isolatedAudioInput(source.isolatedAudioInput), groupKey(source.groupKey),
          synchronization(source.synchronization), audioServoPolicy(source.synchronization.audioServo),
          startup(source.synchronization.startup),
          threadingPolicy(source.threadingPolicy), activationOutputLead(source.activationOutputLead),
          videoFilterActive(source.videoFilterActive), transition(source.transition),
          sourceTiming(source.clock.timing), rtpControlDelivery(source.rtpControlDelivery), m_completeOwner(nullptr), m_fixedOwner(&source) {}
    MediaRealtimeAvSourceFixedView(MediaRealtimeAvSourcePlanningProducts&&) = delete;
    MediaRealtimeAvSourceFixedView(const MediaRealtimeAvSourcePlanningProducts&&) = delete;
    MediaRealtimeAvSourceFixedView(MediaRealtimeAvSourceFixedProducts&&) = delete;
    MediaRealtimeAvSourceFixedView(const MediaRealtimeAvSourceFixedProducts&&) = delete;
    bool belongsTo(const MediaRealtimeAvSourcePlanningProducts& source) const { return m_completeOwner == &source; }
    bool belongsTo(const MediaRealtimeAvSourceFixedProducts& source) const { return m_fixedOwner == &source; }
private:
    const MediaRealtimeAvSourcePlanningProducts* m_completeOwner;
    const MediaRealtimeAvSourceFixedProducts* m_fixedOwner;
};
} // namespace media::ffmpeg::graph
