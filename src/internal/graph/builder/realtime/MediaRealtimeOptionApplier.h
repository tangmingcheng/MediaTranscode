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
    static ::media::Result<void> applyMpegTsDemuxOptions(MediaGraph& graph,
                                                         MediaNodeId nodeId,
                                                         const MediaRealtimeTsInputPlan& plan);
private:
    MediaRealtimeOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
