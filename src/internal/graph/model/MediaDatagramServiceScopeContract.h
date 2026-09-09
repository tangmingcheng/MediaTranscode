#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"

namespace media::ffmpeg::graph {

// One prepared interface service within one controller clock domain. Endpoint
// IDs, output sessions and generations are member-local, not scope identity.
struct MediaDatagramServiceScopeContract final {
    MediaDatagramServiceScopeKind kind;
    std::string scopeId;
    std::string coverageAuthority;
    std::uint64_t capacityWireBytesPerSecond;
    std::string capacityAuthority;
    friend bool operator==(const MediaDatagramServiceScopeContract&,
                           const MediaDatagramServiceScopeContract&) = default;
};

} // namespace media::ffmpeg::graph
