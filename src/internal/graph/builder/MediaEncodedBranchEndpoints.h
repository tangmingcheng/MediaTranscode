#pragma once

#include "internal/graph/builder/MediaEndpoint.h"

namespace media::ffmpeg::graph {

struct MediaEncodedBranchEndpoints final {
    MediaEndpoint codec;
    MediaEndpoint packet;
};

} // namespace media::ffmpeg::graph
