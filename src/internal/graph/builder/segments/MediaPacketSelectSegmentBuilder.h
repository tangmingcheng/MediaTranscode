#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>

namespace media::ffmpeg::graph {

struct PacketSelectOutputPlan final {
    int sourceStreamIndex = invalidMediaStreamIndex;
    MediaEdgeKind edgeKind = MediaEdgeKind::Unknown;
};

struct PacketSelectSegmentOptions {
    std::string prefix = "packet.select";
    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";
    MediaEdgePolicy metadataPolicy;
    MediaEdgePolicy packetPolicy;
    std::optional<PacketSelectOutputPlan> videoOutput;
    std::optional<PacketSelectOutputPlan> audioOutput;
};

struct PacketSelectSegment {
    MediaNodeId demux = MediaNodeId::invalid();
    MediaNodeId split = MediaNodeId::invalid();
    std::optional<MediaEndpoint> videoPacket;
    std::optional<MediaEndpoint> audioPacket;
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
