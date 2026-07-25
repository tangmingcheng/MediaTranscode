#pragma once

#include "internal/graph/core/MediaNodeId.h"

namespace media::ffmpeg::graph {

struct MediaAudioEncodeBranchNodes {
    MediaNodeId packetNormalize = MediaNodeId::invalid();
    MediaNodeId codecResolver = MediaNodeId::invalid();
    MediaNodeId decode = MediaNodeId::invalid();
    MediaNodeId startupTrim = MediaNodeId::invalid();
    MediaNodeId driftController = MediaNodeId::invalid();
    MediaNodeId resample = MediaNodeId::invalid();
    MediaNodeId encode = MediaNodeId::invalid();
    MediaNodeId canonicalizer = MediaNodeId::invalid();
};

} // namespace media::ffmpeg::graph
