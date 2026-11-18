#pragma once

#include "internal/graph/core/MediaNodeId.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

// Exact allocation requests and charged footprints for one shared session and
// its two alternating Data slots. Heap implementation overhead is not included.
struct MediaProtocolGenerationStorageGeometry final {
    MediaSharedAllocationGeometry session;
    std::size_t dataBytes;
    std::size_t dataAlignment;
    std::size_t sessionFootprint;
    std::size_t dataFootprint;
    std::size_t totalBytes;
};

struct MediaProtocolGenerationStorageInstance final {
    MediaNodeId owner;
    MediaNodeKind kind;
    MediaProtocolGenerationStorageGeometry geometry;
};

struct MediaProtocolGenerationStoragePlan final {
    std::vector<MediaProtocolGenerationStorageInstance> instances;
    std::uint64_t admittedBytes = 0;
};

} // namespace media::ffmpeg::graph
