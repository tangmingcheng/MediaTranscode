#pragma once

#include "internal/graph/model/MediaRawRtpPreparationStoragePlan.h"
#include "internal/graph/planner/realtime/MediaRtpReceiveGeometry.h"
#include "media_transcode/Result.h"
#include <array>
#include <span>

namespace media::ffmpeg::graph {

class MediaRawRtpPreparationStoragePlanner final {
public:
    static ::media::Result<MediaRawRtpRuntimeStoragePlan> runtimeStorage(
        std::span<const std::array<MediaRtpReceiveGeometry, 2>* const> streams);
    static ::media::Result<MediaRawRtpPreparationStoragePlan> plan(
        std::size_t maximumObservedWireBytes,
        std::span<const std::size_t> maximumDatagramBytes);
};

} // namespace media::ffmpeg::graph
