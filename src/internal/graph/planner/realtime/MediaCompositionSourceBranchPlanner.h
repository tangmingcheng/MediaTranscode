#pragma once
#include "internal/graph/planner/video/MediaVideoBranchConnectionsPlanner.h"
#include "internal/graph/planner/audio/MediaAudioBranchConnectionsPlanner.h"
#include "internal/graph/model/MediaVideoSourceTiming.h"

namespace media::ffmpeg::graph {
struct MediaCompositionSourceBranchPlan final {
    MediaVideoBranchShape videoDecoderShape;
    MediaVideoBranchShape videoFrameShape;
    std::optional<MediaAudioBranchShape> audioShape;
    std::vector<MediaVideoBranchConnection> videoDecoder;
    std::vector<MediaVideoBranchConnection> videoFrames;
    std::vector<MediaAudioBranchConnection> audio;
};
class MediaCompositionSourceBranchPlanner final {
public:
    // Depends only on source timing/filter/member facts and candidate policies;
    // no sealed admission, graph nodes, or resource allocations are required.
    static ::media::Result<MediaCompositionSourceBranchPlan> plan(
        const MediaVideoSourceTiming& timing, bool filterActive, bool hasAudio,
        const MediaRealtimeEdgePolicySet& policies);
};
} // namespace media::ffmpeg::graph
