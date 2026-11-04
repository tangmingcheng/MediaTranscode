#pragma once

#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/MediaVideoPresentationProgress.h"

namespace media::ffmpeg::graph {

// Stored inline in the bounded codec registry; does not retain a frame/surface.
struct MediaVideoEncodeLineage final {
    std::shared_ptr<const MediaCanonicalLineage> canonical;
    std::optional<MediaVideoPresentationProgress> presentationProgress;
};

} // namespace media::ffmpeg::graph
