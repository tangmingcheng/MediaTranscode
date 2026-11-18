#pragma once

#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"

namespace media::ffmpeg::graph {

// Allocation requests for the actual ownership types. These are not a frame's
// total storage: the planner still needs authoritative reference counts and
// audited vector backing/proxy/growth geometry. In particular, an element's
// footprint must not be multiplied as if each vector element were allocated
// separately. FFmpeg-private and device allocations are outside this layout.
struct MediaFfmpegPayloadOwnershipLayout final {
    // Retained for the consumed frame's ownership lifetime.
    MediaSharedAllocationGeometry references;
    MediaSharedAllocationGeometry completion;
    MediaSharedAllocationGeometry payloadOwner;
    MediaSharedAllocationGeometry referenceElement;
    // Temporary during transactional attachment; overlaps old and new owners.
    MediaSharedAllocationGeometry replacementElement;

    friend bool operator==(const MediaFfmpegPayloadOwnershipLayout&,
        const MediaFfmpegPayloadOwnershipLayout&) = default;
};

} // namespace media::ffmpeg::graph
