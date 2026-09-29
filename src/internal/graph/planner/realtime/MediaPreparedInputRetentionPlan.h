#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/realtime/MediaPreparedInputPayloadEnvelope.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

// A finite admission product, not a guarantee about future UDP arrival rates.
struct MediaPreparedInputRetentionStream final {
    std::size_t maximumUnits;
    std::uint64_t maximumBytes;
    std::uint64_t maximumUnitBytes;
    std::string authority;
};

struct MediaPreparedInputRetentionPlan final {
    MediaRunningTime acquisitionWindow;
    MediaPreparedInputRetentionStream video;
    MediaPreparedInputRetentionStream audio;

    ::media::Status validate() const;
};

class MediaPreparedInputRetentionPlanner final {
public:
    static ::media::Result<MediaPreparedInputRetentionStream> planStream(
        MediaRunningTime acquisitionWindow,
        MediaRational observedAccessUnitRate,
        std::uint64_t replayAccessUnitBound,
        const MediaPreparedInputPayloadBound& payload,
        std::string cadenceAuthority);
};

} // namespace media::ffmpeg::graph
