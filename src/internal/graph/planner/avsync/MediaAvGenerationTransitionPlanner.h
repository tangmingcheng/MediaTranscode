#pragma once

#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/model/MediaAvSyncSourceClockMode.h"
#include "internal/graph/model/MediaVideoStartupReadiness.h"
#include "internal/graph/model/MediaTranscodeParameters.h"

namespace media::ffmpeg::graph {

class MediaAvGenerationTransitionPlanner final {
public:
    static ::media::Result<MediaAvGenerationTransitionPlan> planSourceContribution(
        MediaAvSyncSourceClockMode sourceClockMode,
        MediaBranchMode audioBranchMode,
        bool videoFilterActive,
        MediaVideoStartupReadiness videoReadiness,
        MediaRunningTime acknowledgementTimeout, bool selectedAudio);

    static MediaAvGenerationTransitionPlan plan(
        const std::variant<MediaSeparateRtpOutputRuntimePlan, MediaProjectMpegTsRuntimeOutputPlan>& output,
        MediaAvSyncSourceClockMode sourceClockMode,
        MediaBranchMode audioBranchMode,
        bool videoFilterActive,
        MediaRunningTime acknowledgementTimeout);
};

} // namespace media::ffmpeg::graph
