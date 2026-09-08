#pragma once

#include "application/realtime/MediaRealtimeVideoRunController.h"
#include "media_transcode/Result.h"
#include "media_transcode_beta/realtime.h"

#include <chrono>

namespace media::beta {

class MediaRealtimeBetaOwnedConfig;

class MediaRealtimeBetaSnapshotProjector final {
public:
    MediaRealtimeBetaSnapshotProjector() = delete;

    static mt_beta_realtime_snapshot initial(
        const MediaRealtimeBetaOwnedConfig& config) noexcept;
    static ::media::Status projectPrepared(
        mt_beta_realtime_snapshot& snapshot,
        const ffmpeg::graph::MediaRealtimeVideoPreparedReport& report);
    static ::media::Status projectRuntime(
        mt_beta_realtime_snapshot& snapshot,
        const ffmpeg::graph::MediaGraphRuntimeReport& report,
        std::chrono::milliseconds runningTime);
};

} // namespace media::beta
