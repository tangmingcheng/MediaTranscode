#pragma once

#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

class MediaEncoderRateControlPlanner final {
public:
    static ::media::Result<MediaEncoderRateControlPlan> plan(
        const std::string& encoderName,
        MediaHardwareDeviceKind deviceKind,
        const MediaEncoderRateControlRequest& request);

private:
    MediaEncoderRateControlPlanner() = delete;
};

} // namespace media::ffmpeg::graph
