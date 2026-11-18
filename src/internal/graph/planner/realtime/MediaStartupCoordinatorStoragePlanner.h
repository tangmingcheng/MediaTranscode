#pragma once
#include "media_transcode/Result.h"
#include <cstddef>
#include <optional>

namespace media::ffmpeg::graph {
struct MediaStartupCoordinatorStoragePlan final {
    std::size_t coordinatorBytes;
    std::size_t storeBytes;
    std::size_t snapshotBytes;
    std::size_t selectionBytes;
    std::size_t maximumPriorIds;
    std::size_t maximumResultIds;
    std::size_t priorIdsBytes;
    std::size_t resultIdsBytes;
    std::size_t preparationBytes;
};

// Coordinator-owned objects and synchronous decision backing. Index nodes,
// generation state, release buffers and ordinary configuration strings are
// separate components. Bounds include prior/result replacement overlap.
class MediaStartupCoordinatorStoragePlanner final {
public:
    static ::media::Result<MediaStartupCoordinatorStoragePlan> plan(
        std::size_t videoUnits, std::optional<std::size_t> audioUnits);
};
} // namespace media::ffmpeg::graph
