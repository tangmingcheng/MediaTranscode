#pragma once
#include "internal/graph/planner/realtime/MediaPreparedRtpStorageAdmission.h"

namespace media::ffmpeg::graph {
class MediaPreparedRtpBatchStoragePlanner final {
public:
    static ::media::Result<MediaPreparedRtpBatchStoragePlan> plan(
        std::size_t sourceCount, std::size_t childCount, std::shared_ptr<MediaPreparationMemoryResource> owner);
};
} // namespace media::ffmpeg::graph
