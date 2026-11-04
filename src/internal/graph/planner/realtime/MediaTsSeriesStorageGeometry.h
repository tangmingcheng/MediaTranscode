#pragma once
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"

namespace media::ffmpeg::graph {
// Geometry of the real runtime objects, including allocator-owned shared controls.
// Packet/descriptor element backing and simultaneous ownership bounds are planned separately.
struct MediaTsPacketizerSeriesStorageGeometry final {
    MediaSharedAllocationGeometry packetOwner;
    MediaSharedAllocationGeometry cursor;
    friend bool operator==(const MediaTsPacketizerSeriesStorageGeometry&,
        const MediaTsPacketizerSeriesStorageGeometry&) = default;
};
struct MediaTsProtocolSeriesStorageGeometry final {
    MediaSharedAllocationGeometry batchOwner;
    MediaSharedAllocationGeometry commitOwner;
    MediaSharedAllocationGeometry commitModel;
    friend bool operator==(const MediaTsProtocolSeriesStorageGeometry&,
        const MediaTsProtocolSeriesStorageGeometry&) = default;
};
} // namespace media::ffmpeg::graph
