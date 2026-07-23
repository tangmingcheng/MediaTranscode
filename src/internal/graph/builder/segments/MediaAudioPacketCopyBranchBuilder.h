#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "media_transcode/Result.h"
#include "internal/graph/builder/MediaEncodedBranchEndpoints.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAudioPacketCopyBranchOptions {
    std::string prefix = "audio.copy";
    MediaAudioPipelinePlan plan;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;

    MediaNodeId formatSourceNode = MediaNodeId::invalid();
    std::string formatSourcePort = "format";

    MediaNodeId packetSourceNode = MediaNodeId::invalid();
    std::string packetSourcePort = "audio";

    std::optional<bool> normalizePackets;
};

class MediaAudioPacketCopyBranchBuilder final {
public:
    static ::media::Result<MediaEncodedBranchEndpoints> build(
        MediaGraph& graph,
        const MediaAudioPacketCopyBranchOptions& options);

private:
    MediaAudioPacketCopyBranchBuilder() = default;
};

} // namespace media::ffmpeg::graph
