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

struct MediaVideoTranscodeBranchOptions {
    std::string prefix = "video.transcode";
    MediaPipelinePlan plan;
    MediaVideoTranscodeParameters parameters;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    bool inputStartRequiresKeyFrame = false;
    std::optional<std::size_t> canonicalLineageCapacity;
    std::optional<bool> generationStartRequiresKeyFrame;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "video";

};

class MediaVideoTranscodeBranchBuilder final {
public:
    static ::media::Result<MediaEncodedBranchEndpoints> build(
        MediaGraph& graph,
        const MediaVideoTranscodeBranchOptions& options);

private:
    MediaVideoTranscodeBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph
