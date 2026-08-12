#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/builder/MediaEncodedBranchEndpoints.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaVideoBranchSegmentOptions {
    std::string prefix = "video";
    MediaPipelinePlan plan;
    MediaVideoTranscodeParameters parameters;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    std::optional<MediaVideoLineageEdgePolicySet> lineageEdgePolicies;
    bool inputStartRequiresKeyFrame = false;
    std::optional<std::size_t> canonicalLineageCapacity;
    std::optional<bool> generationStartRequiresKeyFrame;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "video";

    std::optional<bool> normalizePacketCopy;
};

class MediaVideoBranchSegmentBuilder final {
public:
    static ::media::Result<MediaEncodedBranchEndpoints> build(
        MediaGraph& graph,
        const MediaVideoBranchSegmentOptions& options);

private:
    MediaVideoBranchSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
