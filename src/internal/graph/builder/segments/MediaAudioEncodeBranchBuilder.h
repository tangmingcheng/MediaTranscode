#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <string>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaAudioEncodeBranchOptions {
    std::string prefix = "audio.encode";
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
    std::optional<bool> normalizePackets;
    std::optional<MediaAudioCorrectionExecutionMode> correctionMode;
    std::optional<std::uint64_t> correctionGeneration;
    std::optional<std::size_t> correctionLookaheadWindows;
    MediaNodeId correctionSourceNode = MediaNodeId::invalid();
    std::string correctionSourcePort;
};

class MediaAudioEncodeBranchBuilder final {
public:
    static ::media::Result<void> build(MediaGraph& graph,
                                       const MediaAudioEncodeBranchOptions& options);

private:
    MediaAudioEncodeBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph
