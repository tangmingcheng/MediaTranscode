#pragma once

#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaTsDatagramEmissionPlanningFacts final {
    MediaRunningTime videoAccessUnitCadence;
    MediaEncoderRateControlPlan videoRateControl;
    std::optional<MediaRunningTime> audioAccessUnitCadence;
    std::optional<MediaEncoderRateControlPlan> audioRateControl;
    std::uint64_t maximumQueuedBytes;
};

class MediaTsDatagramEmissionPlanner final {
public:
    static ::media::Result<std::int64_t> plannedWireBytesPerSecond(
        std::size_t transportPacketBytes,
        std::size_t maximumPacketsPerDatagram,
        std::size_t perDatagramOverheadBytes,
        MediaRunningTime psiRepeatInterval,
        MediaRunningTime pcrInterval,
        const MediaTsDatagramEmissionPlanningFacts& facts);
    static ::media::Result<MediaRunningTime> maximumResidence(
        const MediaTsDatagramEmissionPlanningFacts& facts);

private:
    MediaTsDatagramEmissionPlanner() = delete;
};

} // namespace media::ffmpeg::graph
