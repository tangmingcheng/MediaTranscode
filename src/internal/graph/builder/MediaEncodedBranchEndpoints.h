#pragma once

#include "internal/graph/builder/MediaEndpoint.h"

#include "internal/graph/model/MediaProcessingNodeOwnership.h"

#include <optional>

namespace media::ffmpeg::graph {

struct MediaEncodedBranchEndpoints final {
    MediaEndpoint codec;
    MediaEndpoint packet;
    std::optional<MediaNodeId> startupPreparationOwner;
    MediaProcessingNodeOwnership processing;
};

} // namespace media::ffmpeg::graph
