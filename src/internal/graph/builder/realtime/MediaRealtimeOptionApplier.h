#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeOptionApplier final {
public:
    static ::media::Result<void> applyInputOptions(MediaGraph& graph,
                                                   MediaNodeId nodeId,
                                                   const MediaRealtimeRtpInputNodePlan& plan);
    static ::media::Result<void> applyOutputOptions(MediaGraph& graph,
                                                    MediaNodeId nodeId,
                                                    const MediaRealtimeRtpOutputNodePlan& plan);
    static ::media::Result<void> applySdpWriterOptions(MediaGraph& graph,
                                                       MediaNodeId nodeId,
                                                       const MediaRealtimeSdpWriterPlan& plan);
    static ::media::Result<void> applyMuxOptions(MediaGraph& graph,
                                                 MediaNodeId nodeId,
                                                 const MediaRealtimeMuxNodePlan& plan);

private:
    MediaRealtimeOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
