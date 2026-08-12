#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/time/MediaDemuxTimestampClockMapper.h"

namespace media::ffmpeg::graph {

struct MediaDecodedDemuxPacketClockBinderNodePlan final {
    MediaScheduledStream stream;
    MediaAvSyncGroupKey groupKey;
    MediaRational streamTimeBase;
    MediaDemuxTimestampClockMapperConfig mapper;
    MediaPreparedDemuxFirstPacketEvidence firstPacket;
};

class MediaDemuxPacketClockBinderNodePlanCodec final {
public:
    static ::media::Status apply(
        MediaGraph& graph,
        MediaNodeId nodeId,
        MediaScheduledStream stream,
        const MediaAvSyncGroupKey& groupKey,
        const MediaDemuxTimestampInputClockAssemblyPlan& plan);

    static ::media::Result<MediaDecodedDemuxPacketClockBinderNodePlan>
    decode(const MediaNode& node);

    static ::media::Result<MediaDemuxTimestampClockMapperConfig>
    mapperConfigFromPlan(const MediaAvSyncPlan& plan);

    static ::media::Status validateAgainstPlanner(
        const MediaDecodedDemuxPacketClockBinderNodePlan& decoded,
        const MediaAvSyncGroupKey& groupKey,
        const MediaAvSyncPlan& plan);

private:
    MediaDemuxPacketClockBinderNodePlanCodec() = delete;
};

} // namespace media::ffmpeg::graph
