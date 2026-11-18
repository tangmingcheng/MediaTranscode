#pragma once

#include "internal/graph/sync/MediaCanonicalSourceStamp.h"
#include "internal/graph/model/MediaVideoCanvasPlan.h"

#include <variant>

namespace media::ffmpeg::graph {

// A displayed decoded picture contributes a point, never an invented source interval.
struct MediaVideoDisplaySourceStamp final {
    MediaSourceAccessUnitIdentity identity;
    std::uint64_t decodedSequence;
    std::uint64_t generation;
    MediaTimeMappingConfidence mappingConfidence;
    MediaRunningTime presentation;
};

struct MediaCanonicalVideoGeneratedBlack final {};

struct MediaCanonicalVideoContribution final {
    std::size_t slot;
    MediaVideoCanvasRectangle rectangle;
    std::variant<MediaCanonicalSourceStamp,
                 MediaCanonicalVideoGeneratedBlack,
                 MediaVideoDisplaySourceStamp> origin;
};

} // namespace media::ffmpeg::graph
