#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourcePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionSourcePlanningRequest final {
    const MediaRealtimeRtpTranscodeRequest& sourceRequest;
    const MediaRealtimeRawInputPlan& raw;
    const MediaVideoSourceTiming& timing;
    const MediaPreparedRtpSourceView& prepared;
    MediaVideoSourcePlan video;
    const MediaResolvedAudioOutputPlan& outputAudio;
    const std::optional<MediaSelectedAudioResampler>& preparedAudioResampler;
    const MediaPreparedRealtimeEmissionSet& emission;
    const MediaRealtimeDeploymentEnvelope& deployment;
    MediaRunningTime activationOutputLead;
    bool selectedAudio;
};

class MediaRealtimeCompositionSourcePlanner final {
public:
    static ::media::Result<MediaRealtimeCompositionSourcePlan> plan(
        MediaRealtimeCompositionSourcePlanningRequest request,
        std::shared_ptr<MediaPreparationMemoryResource> resource);
};

} // namespace media::ffmpeg::graph
