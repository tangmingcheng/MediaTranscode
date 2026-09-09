#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/runtime/factory/MediaRealtimeExecutableGraph.h"
#include "internal/graph/builder/segments/MediaVideoTranscodeBranchBuilder.h"
#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"
#include "media_transcode/Result.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoEncodingGroupContract.h"
#include "internal/graph/model/MediaGraphPayloadRetentionGrowth.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeVideoEncodingSegmentGraph final {
    std::vector<MediaNodeId> nodeIds;
    MediaThreadingPolicy threading;
    MediaFinalGraphResourceLedger resources;
    MediaGraphPayloadRetentionGrowth sourceRetentionGrowth;
    MediaEncodedBranchEndpoints encoded;
};

struct MediaRealtimeVideoProtocolOutputGraph final {
    std::shared_ptr<const MediaGraph> graph;
    std::vector<MediaNodeId> nodeIds;
    MediaThreadingPolicy threading;
    std::uint64_t fixedStorageBytes;
    MediaGraphPayloadRetentionGrowth encodedRetentionGrowth;
};

struct MediaRealtimeInitialVideoOutputTopology final {
    std::vector<MediaNodeId> sharedNodeIds;
    std::vector<MediaNodeId> encodingNodeIds;
    std::vector<MediaNodeId> outputNodeIds;
    MediaNodeId sourceFanout;
    MediaEncodedBranchEndpoints encoded;
};

struct MediaRealtimeVideoEncodingGroupGraph final {
    MediaGraph graph;
    MediaRealtimeVideoEncodingSegmentGraph segment;
};

class MediaRealtimeRtpTranscodeGraphBuilder final {
public:
    static ::media::Result<MediaRealtimeInitialVideoOutputTopology> initialOutputTopology(const MediaGraph& graph);
    static ::media::Result<MediaGraph> build(const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaGraph> build(MediaRealtimeRtpTranscodePlan plan);
    static ::media::Result<MediaRealtimeExecutableGraph> buildExecutable(
        MediaRealtimeTranscodePreflight preflight);
    static ::media::Status validate(const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaRealtimeVideoEncodingGroupGraph> appendEncodingGroup(
        MediaGraph graph,
        const MediaRealtimeRtpTranscodePlan& plan,
        const std::string& prefix,
        MediaEndpoint formatSource,
        MediaSharedVideoDecodeEndpoints sharedDecode);

    static ::media::Result<MediaRealtimeVideoProtocolOutputGraph> appendProtocolOutput(
        MediaGraph graph,
        const MediaRealtimeRtpTranscodePlan& plan,
        const std::string& prefix,
        MediaEncodedBranchEndpoints encoded);

private:
    static ::media::Result<MediaGraph> buildPlanned(
        MediaRealtimeRtpTranscodePlan& plan);

    MediaRealtimeRtpTranscodeGraphBuilder() = default;
};

} // namespace media::ffmpeg::graph
