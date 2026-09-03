#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/realtime/MediaPreparedRtpAccessUnitEnvelope.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaRtpInputPlayoutPlan final {
    MediaRunningTime latency = MediaRunningTime::fromNanoseconds(0);
    std::size_t startupAccessUnits = 0;
    std::size_t maximumRetainedAccessUnits = 0;
    std::uint64_t maximumRetainedPayloadBytes = 0;
    std::string authority;

    ::media::Status validate() const;
};

class MediaRtpInputPlayoutPlanner final {
public:
    static ::media::Result<MediaRtpInputPlayoutPlan> plan(
        MediaRunningTime latency,
        MediaRational accessUnitRate,
        const MediaPreparedRtpAccessUnitEnvelope& accessUnitEnvelope);

private:
    MediaRtpInputPlayoutPlanner() = delete;
};

} // namespace media::ffmpeg::graph
