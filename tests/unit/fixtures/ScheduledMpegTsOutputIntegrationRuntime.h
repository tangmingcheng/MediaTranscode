#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "media_transcode/Result.h"

#include <filesystem>

namespace media_transcode::test {

class ScheduledMpegTsDecodeSampleFixture;

class ScheduledMpegTsOutputIntegrationRuntime final {
public:
    static ::media::Status write(
        const ::media::ffmpeg::graph::MediaRealtimeAvSyncRuntimePlan& plan,
        const ::media::ffmpeg::graph::MediaPlaybackEpoch& epoch,
        ScheduledMpegTsDecodeSampleFixture& sample);

private:
    ScheduledMpegTsOutputIntegrationRuntime() = delete;
};

} // namespace media_transcode::test
