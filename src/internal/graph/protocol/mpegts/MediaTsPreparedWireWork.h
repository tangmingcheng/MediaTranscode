#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

// Serialized payload bytes include transport framing, but not network overhead.
struct MediaTsPreparedControlGeometry final {
    std::uint64_t endpointId;
    std::uint64_t payloadBytes;
    MediaRunningTime release;
    MediaRunningTime deadline;
};

struct MediaTsPreparedMediaGeometry final {
    std::uint64_t endpointId;
    std::uint64_t regularPayloadBytes;
    std::uint64_t lastPayloadBytes;
    std::size_t count;
    MediaRunningTime release;
    MediaRunningTime deadline;
};

struct MediaTsPreparedWireWork final {
    std::size_t protocolBegin;
    MediaTsPreparedMediaGeometry media;
    std::optional<MediaTsPreparedControlGeometry> leadingReport;
};

struct MediaTsBoundWireRange final {
    std::uint64_t generation;
    std::uint64_t identity;
    std::uint64_t firstSequence;
    std::uint64_t firstMediaSequence;
    std::uint64_t lastSequence;
    std::size_t protocolBegin;
    std::size_t protocolEnd;
};

struct MediaTsBoundWireWork final {
    MediaTsPreparedWireWork work;
    MediaTsBoundWireRange range;
};

} // namespace media::ffmpeg::graph
