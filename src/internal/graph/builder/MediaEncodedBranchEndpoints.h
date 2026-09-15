#pragma once

#include "internal/graph/builder/MediaEndpoint.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaEncodedBranchEndpoints final {
    MediaEndpoint codec;
    MediaEndpoint packet;
    std::optional<MediaNodeId> startupPreparationOwner;
};

} // namespace media::ffmpeg::graph
