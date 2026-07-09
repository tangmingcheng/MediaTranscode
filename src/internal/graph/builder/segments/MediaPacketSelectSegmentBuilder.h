#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct PacketSelectSegmentOptions {
    std::string prefix = "packet.select";
    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
};

struct PacketSelectSegment {
    MediaNodeId demux = MediaNodeId::invalid();
    MediaNodeId split = MediaNodeId::invalid();
};

class MediaPacketSelectSegmentBuilder final {
public:
    static ::media::Result<PacketSelectSegment> buildDemuxStreamSplit(
        MediaGraph& graph,
        const PacketSelectSegmentOptions& options);

private:
    MediaPacketSelectSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
