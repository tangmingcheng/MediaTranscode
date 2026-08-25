#pragma once

#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/planner/MediaPreparedEncoderEmissionEnvelope.h"
#include "media_transcode/Result.h"

#include <string>

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaEncoderEmissionPreflightAdapter final {
public:
    static ::media::Status applyBeforeOpen(
        AVCodecContext& context,
        const MediaEncoderRateControlPlan& contract);

    static ::media::Result<MediaPreparedEncoderEmissionEnvelope> readAfterOpen(
        const AVCodecContext& context,
        const MediaEncoderRateControlPlan& contract,
        MediaRational plannedCadence,
        MediaEncodedPacketLayout packetLayout,
        std::string authority,
        std::string backend);

private:
    MediaEncoderEmissionPreflightAdapter() = delete;
};

} // namespace media::ffmpeg::graph
