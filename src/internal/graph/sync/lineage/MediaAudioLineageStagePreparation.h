#pragma once

#include "internal/graph/core/MediaNode.h"
#include "media_transcode/Result.h"
#include <cstddef>
#include <string_view>

namespace media::ffmpeg::graph {

struct MediaAudioLineageStagePreparation final {
    bool synchronized = false;
    std::size_t capacity = 0;
};

::media::Result<MediaAudioLineageStagePreparation>
prepareMediaAudioLineageStage(
    const MediaNode& node,
    std::string_view expectedIdentity);

} // namespace media::ffmpeg::graph
