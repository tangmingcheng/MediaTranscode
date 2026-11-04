#pragma once

#include "internal/graph/time/MediaRunningTime.h"

#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

enum class MediaWireMediaBoundary : std::uint8_t {
    None,
    VideoRandomAccessUnitEnd
};

// Diagnostic provenance only. Scheduling uses canonicalDeadline below.
struct MediaWireProtocolDeadlineObservation final {
    MediaRunningTime originalDeadline;
    MediaRunningTime prefixCompletionLimit;
    friend bool operator==(const MediaWireProtocolDeadlineObservation&,
                           const MediaWireProtocolDeadlineObservation&) = default;
};

struct MediaWireDatagramLayout {
    std::uint64_t generation;
    std::uint64_t endpointId;
    std::uint64_t payloadOffset;
    std::uint64_t payloadSize;
    MediaRunningTime canonicalRelease;
    MediaRunningTime canonicalDeadline;
    MediaWireMediaBoundary mediaBoundary;
    std::optional<MediaWireProtocolDeadlineObservation> protocolDeadlineObservation;

    friend bool operator==(const MediaWireDatagramLayout&,
                           const MediaWireDatagramLayout&) = default;
};

struct MediaWireDatagramDescriptor final : MediaWireDatagramLayout {
    std::uint64_t globalSequence;
    friend bool operator==(const MediaWireDatagramDescriptor&,
                           const MediaWireDatagramDescriptor&) = default;
};

} // namespace media::ffmpeg::graph
