#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaVideoBranchSegmentOptions {
    std::string prefix = "video";
    MediaPipelinePlan plan;
    MediaVideoTranscodeParameters parameters;
    MediaGraphQueueParameters queues;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "video";

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort = "codec";
    std::string muxPacketPort = "packet";
};

class MediaVideoBranchSegmentBuilder final {
public:
    static ::media::Result<bool> buildIfPlanned(MediaGraph& graph,
                                                const MediaVideoBranchSegmentOptions& options);

private:
    MediaVideoBranchSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
