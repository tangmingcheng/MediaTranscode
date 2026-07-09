#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioBranchSegmentOptions {
    std::string prefix = "audio";
    MediaAudioPipelinePlan plan;
    MediaAudioTranscodeParameters parameters;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "audio";

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort = "codec";
    std::string muxPacketPort = "packet";
};

class MediaAudioBranchSegmentBuilder final {
public:
    static ::media::Result<bool> buildIfPlanned(MediaGraph& graph,
                                                const MediaAudioBranchSegmentOptions& options);

private:
    MediaAudioBranchSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
