#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"

namespace media::ffmpeg::graph {

struct MediaDecodedProjectMpegTsPlanSourceNodePlan final {
    MediaProtocolOutputSessionKey sessionKey;
    MediaTranscodeStreamSet streamSet;
    MediaProjectMpegTsRuntimeOutputPlan outputPlan;
};

class MediaProjectMpegTsPlanSourceNodePlanCodec final {
public:
    static ::media::Status apply(MediaGraph& graph,
                                 MediaNodeId nodeId,
                                 const MediaProtocolOutputSessionKey& sessionKey,
                                 MediaTranscodeStreamSet streamSet,
                                 const MediaProjectMpegTsRuntimeOutputPlan&
                                     outputPlan);
    static ::media::Result<MediaDecodedProjectMpegTsPlanSourceNodePlan> decode(
        const MediaNode& node);
    static ::media::Status validateAgainstPlanner(
        const MediaDecodedProjectMpegTsPlanSourceNodePlan& decoded,
        const MediaProtocolOutputSessionKey& plannerSession,
        MediaTranscodeStreamSet plannerStreamSet,
        const MediaProjectMpegTsRuntimeOutputPlan& plannerProduct);

private:
    MediaProjectMpegTsPlanSourceNodePlanCodec() = delete;
};

} // namespace media::ffmpeg::graph
