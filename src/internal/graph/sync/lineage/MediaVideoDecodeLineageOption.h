#pragma once
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaVideoDecodeLineageProduct.h"
#include <optional>

namespace media::ffmpeg::graph {
::media::Result<std::optional<MediaVideoDecodeLineageProduct>>
parseMediaVideoDecodeLineageOption(const MediaNodeOptions* options, bool copyOpaque);
} // namespace media::ffmpeg::graph
