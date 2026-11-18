#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/validation/MediaAvDomainValidationView.h"

namespace media::ffmpeg::graph {

class MediaAvRuntimeRegistrationValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph, const MediaAvDomainValidationView& binding);
};

} // namespace media::ffmpeg::graph
