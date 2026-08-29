#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/mpegts/MediaMpegTsTimingPolicy.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

class MediaMpegTsOutputTimingPlanner final {
public:
    static ::media::Result<MediaMpegTsTimingPolicy> planVariableBitrate(
        MediaRunningTime maximumReleaseJitter,
        std::string releaseJitterAuthority,
        MediaRational videoCadence);

    static ::media::Result<MediaRunningTime> startupEmissionPreroll(
        MediaRunningTime transportDecodeLead,
        MediaRational videoCadence,
        std::optional<MediaRunningTime> audioCadence,
        const MediaMpegTsTimingPolicy& timingPolicy);

private:
    MediaMpegTsOutputTimingPlanner() = delete;
};

} // namespace media::ffmpeg::graph
