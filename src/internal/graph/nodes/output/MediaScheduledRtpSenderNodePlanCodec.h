#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

struct MediaDecodedScheduledRtpSenderNodePlan final {
    MediaAvSyncGroupKey groupKey;
    MediaScheduledRtpOutputPlan output;
    MediaSeparateRtpSdpRuntimePlan sdp;
};

class MediaScheduledRtpSenderNodePlanCodec final {
public:
    static ::media::Status apply(
        MediaGraph& graph,
        MediaNodeId nodeId,
        const MediaAvSyncGroupKey& groupKey,
        const MediaScheduledRtpOutputPlan& output,
        const MediaSeparateRtpSdpRuntimePlan& sdp);

    static ::media::Result<MediaDecodedScheduledRtpSenderNodePlan> decode(
        const MediaNode& node);

private:
    MediaScheduledRtpSenderNodePlanCodec() = delete;
};

} // namespace media::ffmpeg::graph
