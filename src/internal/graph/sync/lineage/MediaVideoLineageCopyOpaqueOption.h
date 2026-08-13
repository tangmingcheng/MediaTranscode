#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {

::media::Result<bool> parseMediaVideoLineageCopyOpaqueOption(
    const MediaNodeOptions* options,
    std::string_view optionName);

} // namespace media::ffmpeg::graph
