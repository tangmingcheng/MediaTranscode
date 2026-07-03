#pragma once

#include "internal/graph/core/MediaNodeId.h"

namespace media::ffmpeg::graph {

struct MediaVideoTranscodeBranchNodes {
    MediaNodeId codecResolver = MediaNodeId::invalid();
    MediaNodeId videoDecode = MediaNodeId::invalid();
    MediaNodeId hardwareTransfer = MediaNodeId::invalid();
    MediaNodeId videoTimestamp = MediaNodeId::invalid();
    MediaNodeId videoFrameRate = MediaNodeId::invalid();
    MediaNodeId videoFilter = MediaNodeId::invalid();
    MediaNodeId videoEncode = MediaNodeId::invalid();
};

} // namespace media::ffmpeg::graph
