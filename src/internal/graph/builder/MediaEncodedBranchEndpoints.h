#pragma once

#include "internal/graph/builder/MediaEndpoint.h"

#include "internal/graph/model/MediaProcessingNodeOwnership.h"

#include <optional>
#include <variant>
#include "internal/graph/builder/segments/MediaBranchConnectionBindings.h"

namespace media::ffmpeg::graph {

struct MediaEncodedBranchEndpoints final {
    MediaEndpoint codec;
    MediaEndpoint packet;
    std::optional<MediaNodeId> startupPreparationOwner;
    MediaProcessingNodeOwnership processing;
    std::optional<std::variant<MediaVideoBranchBindings, MediaAudioBranchBindings>> bindings;
};

template<class Bindings>
struct MediaSourceBranchEndpoints final {
    MediaEndpoint frame;
    MediaNodeId codecResolver;
    std::optional<MediaNodeId> startupPreparationOwner;
    MediaProcessingNodeOwnership processing;
    Bindings bindings;
};
using MediaVideoSourceBranchEndpoints = MediaSourceBranchEndpoints<MediaVideoSourceBranchBindings>;
using MediaAudioSourceBranchEndpoints = MediaSourceBranchEndpoints<MediaAudioBranchBindings>;

struct MediaOutputEncoderEndpoints final {
    MediaEndpoint frameInput;
    MediaEndpoint codec;
    MediaEncodedBranchEndpoints encoded;
    MediaNodeId codecResolver;
};

} // namespace media::ffmpeg::graph
