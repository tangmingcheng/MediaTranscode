#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct LocalFilePlannerNodeIds {
    MediaNodeId codecResolver;
    MediaNodeId videoDecode;
    MediaNodeId hardwareTransfer;
    MediaNodeId videoTimestamp;
    MediaNodeId videoFrameRate;
    MediaNodeId videoFilter;
    MediaNodeId videoEncode;
};

::media::Status applySelectedVideoPlanOptions(MediaGraph& graph,
                                              const LocalFilePlannerNodeIds& nodes,
                                              const MediaPipelinePlan& plan);

} // namespace media::ffmpeg::graph
