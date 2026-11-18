#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include <variant>

namespace media::ffmpeg::graph {

struct MediaObservedVideoCadence final {
    MediaRational rate;
};
struct MediaSourcePresentationTiming final {};
using MediaVideoSourceTiming = std::variant<
    MediaObservedVideoCadence, MediaSourcePresentationTiming>;

} // namespace media::ffmpeg::graph
