#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/sync/MediaAudioCorrection.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/sync/lineage/MediaAudioLineageExecutionMode.h"
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

    std::optional<bool> normalizeInputPackets;
    std::optional<MediaAudioCorrectionExecutionMode> correctionMode;
    std::optional<MediaAudioLineageExecutionMode> lineageMode;
    std::optional<std::size_t> lineageCapacity;
    std::optional<std::uint64_t> correctionGeneration;
    std::optional<std::size_t> correctionLookaheadWindows;
    std::optional<MediaAvSyncGroupKey> syncGroup;
};

struct MediaAudioBranchSegmentResult final {
    bool built = false;
    MediaEndpoint codec;
    MediaEndpoint packet;
};

class MediaAudioBranchSegmentBuilder final {
public:
    static ::media::Result<MediaAudioBranchSegmentResult> buildIfPlanned(
        MediaGraph& graph,
        const MediaAudioBranchSegmentOptions& options);

private:
    MediaAudioBranchSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
