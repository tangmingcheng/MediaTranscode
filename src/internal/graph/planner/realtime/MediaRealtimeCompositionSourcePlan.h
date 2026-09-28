#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"

namespace media::ffmpeg::graph {

// Source preparation owns no encoder, muxer, sender or output FIFO product.
struct MediaRealtimeCompositionSourcePlan final {
    RealtimeInputType inputType;
    MediaRealtimeRtpInputNodePlan input;
    int videoStreamIndex;
    MediaVideoSourcePlan video;
    MediaRational frameRate;
    std::optional<MediaRational> maximumFrameDuplicationGap;
    MediaRealtimeAvSourceRuntimePlan runtime;
};

} // namespace media::ffmpeg::graph
