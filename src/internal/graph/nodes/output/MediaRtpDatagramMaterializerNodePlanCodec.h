#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"

namespace media::ffmpeg::graph {

struct MediaDecodedScheduledRtpSenderNodePlan final {
    MediaProtocolOutputSessionKey sessionKey;
    MediaTranscodeStreamSet streamSet;
    MediaScheduledRtpOutputPlan output;
    MediaSeparateRtpSdpRuntimePlan sdp;
};

class MediaRtpDatagramMaterializerNodePlanCodec final {
public:
    static ::media::Status apply(
        MediaGraph& graph,
        MediaNodeId nodeId,
        const MediaProtocolOutputSessionKey& sessionKey,
        MediaTranscodeStreamSet streamSet,
        const MediaScheduledRtpOutputPlan& output,
        const MediaSeparateRtpSdpRuntimePlan& sdp);

    static ::media::Result<MediaDecodedScheduledRtpSenderNodePlan> decode(
        const MediaNode& node);

private:
    MediaRtpDatagramMaterializerNodePlanCodec() = delete;
};

} // namespace media::ffmpeg::graph
