#pragma once

#include "media_transcode/Result.h"
#include "internal/graph/core/MediaNodeId.h"

#include <span>

namespace media::ffmpeg::graph {

class MediaGraph;
struct MediaRealtimeVideoRuntimeBinding;
struct MediaRealtimeVideoRuntimePlan;
struct MediaRealtimeVideoSessionFacts;

class MediaRealtimeVideoGraphShapeValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph,
        const MediaRealtimeVideoRuntimeBinding& binding);
    static ::media::Status validateAbsent(const MediaGraph& graph);
    static ::media::Status validateOutputBranch(
        const MediaGraph& graph,
        std::span<const MediaNodeId> outputNodes,
        const MediaRealtimeVideoSessionFacts& sharedInput,
        const MediaRealtimeVideoRuntimePlan& output);

private:
    MediaRealtimeVideoGraphShapeValidator() = delete;
};

} // namespace media::ffmpeg::graph
