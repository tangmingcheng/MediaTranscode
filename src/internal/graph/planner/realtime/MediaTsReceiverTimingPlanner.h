#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/protocol/mpegts/MediaMpegTsTimingPolicy.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaTsReceiverTimingPlanner final {
public:
    static ::media::Result<MediaMpegTsTimingPolicy> plan(
        MediaRunningTime receiverTransportDecodeLead,
        std::string receiverAuthority,
        MediaRunningTime targetResidence,
        MediaRunningTime maximumResidence,
        MediaRunningTime maximumReleaseJitter,
        std::string releaseJitterAuthority,
        MediaRational videoCadence,
        std::optional<MediaRunningTime> audioCadence);

    static ::media::Result<MediaRunningTime> startupEmissionPreroll(
        MediaRunningTime receiverTransportDecodeLead,
        MediaRational videoCadence,
        std::optional<MediaRunningTime> audioCadence,
        const MediaMpegTsTimingPolicy& timingPolicy);

private:
    MediaTsReceiverTimingPlanner() = delete;
};

} // namespace media::ffmpeg::graph
