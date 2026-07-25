#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsResolvedPipelineFacts.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAvSyncPlanner final {
public:
    static ::media::Result<MediaAvSyncPlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaTsSelectedProgramPlan* selectedTsProgram = nullptr,
        const MediaProjectMpegTsResolvedPipelineFacts* resolvedTsFacts = nullptr);

private:
    MediaAvSyncPlanner() = delete;
};

} // namespace media::ffmpeg::graph
