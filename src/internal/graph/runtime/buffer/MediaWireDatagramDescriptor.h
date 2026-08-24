#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaWireDatagramDescriptor final {
    std::uint64_t generation;
    std::uint64_t endpointId;
    std::uint64_t payloadOffset;
    std::uint64_t payloadSize;
    MediaRunningTime canonicalRelease;
    MediaRunningTime canonicalDeadline;
    std::uint64_t globalSequence;

    friend bool operator==(const MediaWireDatagramDescriptor&,
                           const MediaWireDatagramDescriptor&) = default;
};

} // namespace media::ffmpeg::graph
