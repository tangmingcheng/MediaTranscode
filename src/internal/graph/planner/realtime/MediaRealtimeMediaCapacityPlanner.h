#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRealtimeMediaCapacityPlan final {
    std::size_t videoUnits;
    std::uint64_t videoUnitBytes;
    std::uint64_t videoBytes;
    std::optional<std::size_t> audioUnits;
    std::optional<std::uint64_t> audioUnitBytes;
    std::optional<std::uint64_t> audioBytes;
    MediaRunningTime maximumGap;
};

class MediaRealtimeMediaCapacityPlanner final {
public:
    static ::media::Result<MediaRealtimeMediaCapacityPlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeMediaCapacityPlanner() = delete;
};

} // namespace media::ffmpeg::graph
