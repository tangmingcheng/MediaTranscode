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

struct MediaSourceBranchEndpoints final {
    MediaEndpoint frame;
    MediaNodeId codecResolver;
    std::optional<MediaNodeId> startupPreparationOwner;
    MediaProcessingNodeOwnership processing;
};

struct MediaOutputEncoderEndpoints final {
    MediaEndpoint frameInput;
    MediaEndpoint codec;
    MediaEncodedBranchEndpoints encoded;
    MediaNodeId codecResolver;
};

} // namespace media::ffmpeg::graph
