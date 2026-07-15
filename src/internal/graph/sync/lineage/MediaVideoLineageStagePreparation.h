#pragma once

#include "internal/graph/core/MediaNode.h"
#include "internal/graph/sync/lineage/MediaCodecLineageRegistry.h"

#include <memory>
#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

::media::Result<std::optional<std::size_t>>
prepareMediaVideoLineageStageCapacity(
    const MediaNode& node,
    std::string_view expectedIdentity);

::media::Result<std::shared_ptr<MediaCodecLineageRegistry>>
prepareMediaVideoLineageStage(
    const MediaNode& node,
    std::string_view expectedIdentity);

} // namespace media::ffmpeg::graph
