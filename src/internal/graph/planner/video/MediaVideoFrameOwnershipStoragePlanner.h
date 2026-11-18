#pragma once

#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"
#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaVideoFrameOwnershipStorageFacts final {
    // Includes engine-added references, not merely FFmpeg's video data planes.
    // This is an upstream contract; an observed first frame is not an upper bound.
    std::size_t maximumExtendedBufferReferences;
    bool hasHardwareFramesContext;
    MediaVideoFrameReadiness::Kind readinessKind;
};

// Per-operation components, not a simultaneous frame or graph storage total.
// A caller must account for shared aliases, attachment layers, retained input
// snapshots and overlapping operations before combining these components.
// Lineage allocation requests include the preparation resource's real headers.
// AVBufferRef contributes only its public logical sizeof: FFmpeg allocates those
// headers separately, and this product does not reserve or migrate that storage.
struct MediaVideoFrameOwnershipStoragePlan final {
    std::size_t maximumBufferReferences;
    // One retained capture owner, its vector and referenced public buffer headers.
    std::size_t referenceSnapshotBytes;
    // One attachment layer; each reference has its own retained and wrapper head.
    std::size_t payloadAttachmentBytes;
    // One temporary replacement vector, overlapping the old and new owners.
    std::size_t payloadAttachmentScratchBytes;
    // CUDA operation plus one device reference; zero for completed readiness.
    std::size_t readinessOperationBytes;
    std::size_t completionBytes;
    bool operator==(const MediaVideoFrameOwnershipStoragePlan&) const noexcept = default;
};

class MediaVideoFrameOwnershipStoragePlanner final {
public:
    static ::media::Result<MediaVideoFrameOwnershipStoragePlan> plan(
        const MediaVideoFrameOwnershipStorageFacts& facts);
};

} // namespace media::ffmpeg::graph
