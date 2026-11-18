#pragma once

#include "internal/graph/sync/MediaCanonicalIdentity.h"
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaRtpVideoTimingProduct { CanonicalInterval, SourcePresentation };

// Source presentation is an AU identity, not a complete display interval.
// Its identity is supplied by the planner before raw ingress provenance retires.
struct MediaRtpVideoTimingPlan final {
    MediaRtpVideoTimingProduct product;
    std::optional<MediaCanonicalIdentity> sourceIdentity;

    bool operator==(const MediaRtpVideoTimingPlan&) const = default;
};

} // namespace media::ffmpeg::graph
