#pragma once

#include "internal/graph/sync/MediaCanonicalSourceStamp.h"
#include "internal/graph/model/MediaVideoCanvasPlan.h"

#include <variant>

namespace media::ffmpeg::graph {

struct MediaCanonicalVideoGeneratedBlack final {};

struct MediaCanonicalVideoContribution final {
    std::size_t slot;
    MediaVideoCanvasRectangle rectangle;
    std::variant<MediaCanonicalSourceStamp,
                 MediaCanonicalVideoGeneratedBlack> origin;
};

} // namespace media::ffmpeg::graph
