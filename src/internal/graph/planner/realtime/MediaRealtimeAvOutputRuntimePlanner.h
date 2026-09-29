#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvOutputRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeOutputPlanningDraft;

struct MediaRealtimeAvOutputRuntimeRequest final {
    const MediaAvSyncGroupKey& groupKey;
    const MediaResolvedAudioOutputPlan& audio;
    const MediaGraphQueueParameters& queues;
    const MediaRealtimeEdgePolicySet& edgePolicies;
    const MediaRealtimeAvOutputTimingFacts& timing;
    std::optional<std::int64_t> maximumInputAudioSamples;
    RealtimeOutputStreamLayout layout;
    MediaOutputTransportKind transport;
    MediaRational videoFrameRate;
    const MediaRealtimeDeploymentEnvelope& deployment;
    const MediaPreparedRealtimeEmissionSet& emission;
};

class MediaRealtimeAvOutputRuntimePlanner final {
public:
    static ::media::Result<MediaRealtimeAvOutputRuntimePlan> plan(
        const MediaRealtimeAvOutputRuntimeRequest& request,
        MediaAvSyncPlan synchronization,
        MediaRealtimeOutputPlanningDraft& output);
private:
    MediaRealtimeAvOutputRuntimePlanner() = delete;
};

} // namespace media::ffmpeg::graph
