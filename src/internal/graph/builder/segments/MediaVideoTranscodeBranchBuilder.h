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

struct MediaSharedVideoDecodeEndpoints final {
    MediaEndpoint frame;
    MediaEndpoint codec;
};

struct MediaVideoBranchConnectionOptions {
    std::string prefix = "video.transcode";
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

    std::optional<MediaSharedVideoDecodeEndpoints> sharedDecode;

};

struct MediaVideoTranscodeBranchOptions : MediaVideoBranchConnectionOptions {
    MediaPipelinePlan plan;
    MediaVideoTranscodeParameters parameters;
};

struct MediaVideoSourceBranchOptions : MediaVideoBranchConnectionOptions {
    MediaVideoSourcePlan plan;
    int sourceStreamIndex;
    MediaRational frameRate;
    std::optional<MediaRational> maximumFrameDuplicationGap;
};

class MediaVideoTranscodeBranchBuilder final {
public:
    static ::media::Result<MediaEncodedBranchEndpoints> build(
        MediaGraph& graph,
        const MediaVideoTranscodeBranchOptions& options);

    static ::media::Result<MediaSourceBranchEndpoints> buildSource(
        MediaGraph& graph, const MediaVideoSourceBranchOptions& options,
        MediaEndpoint outputEncoderCodec);
    static ::media::Result<MediaOutputEncoderEndpoints> buildOutputEncoder(
        MediaGraph& graph, const MediaVideoTranscodeBranchOptions& options);

private:
    MediaVideoTranscodeBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph
