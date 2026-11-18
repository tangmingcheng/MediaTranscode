#pragma once

#include "internal/graph/planner/realtime/MediaTsSeriesStorageGeometry.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"

#include <optional>

namespace media::ffmpeg::graph {

// The initial reservation already owns its commit model. Later reservations
// replace that model only after the previous prefix has fully committed.
struct MediaTsPrefixStoragePreparation final {
    MediaOwnedMemoryAllocation view;
    std::optional<MediaOwnedMemoryAllocation> commitModel;

    static ::media::Result<MediaTsPrefixStoragePreparation> prepare(
        const std::shared_ptr<MediaNetworkMemoryResource>& resource,
        const MediaTsProtocolSeriesStorageGeometry& geometry,
        bool needsCommitModel);
};

} // namespace media::ffmpeg::graph
