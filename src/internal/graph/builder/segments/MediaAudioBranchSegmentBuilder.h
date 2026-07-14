#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioBranchSegmentOptions {
    std::string prefix = "audio";
    MediaAudioPipelinePlan plan;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "audio";

    MediaNodeId muxNode = MediaNodeId::invalid();
    std::string muxCodecPort = "codec";
    std::string muxPacketPort = "packet";
    std::optional<bool> normalizeInputPackets;
    std::optional<MediaAudioCorrectionExecutionMode> correctionMode;
    std::optional<std::uint64_t> correctionGeneration;
    std::optional<std::size_t> correctionLookaheadWindows;
    MediaNodeId correctionSourceNode = MediaNodeId::invalid();
    std::string correctionSourcePort;
};

class MediaAudioBranchSegmentBuilder final {
public:
    static ::media::Result<bool> buildIfPlanned(MediaGraph& graph,
                                                const MediaAudioBranchSegmentOptions& options);

private:
    MediaAudioBranchSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
