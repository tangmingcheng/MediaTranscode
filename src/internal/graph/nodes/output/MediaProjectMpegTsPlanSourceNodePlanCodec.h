#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

struct MediaDecodedProjectMpegTsPlanSourceNodePlan final {
    MediaAvSyncGroupKey groupKey;
    MediaProjectMpegTsRuntimeOutputPlan outputPlan;
};

class MediaProjectMpegTsPlanSourceNodePlanCodec final {
public:
    static ::media::Status apply(MediaGraph& graph,
                                 MediaNodeId nodeId,
                                 const MediaAvSyncGroupKey& groupKey,
                                 const MediaProjectMpegTsRuntimeOutputPlan&
                                     outputPlan);
    static ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan> decode(
        const MediaNode& node);

private:
    MediaProjectMpegTsPlanSourceNodePlanCodec() = delete;
};

} // namespace media::ffmpeg::graph
