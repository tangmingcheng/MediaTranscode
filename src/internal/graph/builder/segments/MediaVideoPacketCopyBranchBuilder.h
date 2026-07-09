#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaVideoPacketCopyBranchOptions {
    std::string prefix = "video.copy";
    MediaPipelinePlan plan;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "video";

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort = "codec";
    std::string muxPacketPort = "packet";
};

class MediaVideoPacketCopyBranchBuilder final {
public:
    static ::media::Result<void> build(MediaGraph& graph,
                                       const MediaVideoPacketCopyBranchOptions& options);

private:
    MediaVideoPacketCopyBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph
