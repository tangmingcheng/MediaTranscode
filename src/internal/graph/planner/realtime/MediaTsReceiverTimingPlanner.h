#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaTsReceiverTimingPlanner final {
public:
    static constexpr MediaRunningTime pcrInterval() noexcept
    {
        return MediaRunningTime::fromNanoseconds(20'000'000);
    }

    static ::media::Result<MediaRunningTime> startupEmissionPreroll(
        MediaRunningTime receiverTransportDecodeLead,
        MediaRational videoCadence,
        std::optional<MediaRunningTime> audioCadence);

private:
    MediaTsReceiverTimingPlanner() = delete;
};

} // namespace media::ffmpeg::graph
