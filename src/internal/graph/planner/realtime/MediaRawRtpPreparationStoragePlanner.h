#pragma once

#include "internal/graph/model/MediaRawRtpPreparationStoragePlan.h"
#include "media_transcode/Result.h"
#include <span>

namespace media::ffmpeg::graph {

class MediaRawRtpPreparationStoragePlanner final {
public:
    static ::media::Result<MediaRawRtpPreparationStoragePlan> plan(
        std::size_t maximumObservedWireBytes,
        std::span<const std::size_t> maximumDatagramBytes);
};

} // namespace media::ffmpeg::graph
