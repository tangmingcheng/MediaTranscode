#pragma once

#include "internal/graph/planner/video/MediaPipelineStagePlan.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <optional>
#include <string>

struct AVCodecContext;

namespace media::ffmpeg::graph {

// Inspects a disposable opened encoder, without a source decoder or pipeline.
// Packet-layout probing can submit frames and enter drain. The caller must not
// transfer this context to production after inspection.
class MediaOpenedVideoEncoderProbe final {
public:
    static ::media::Status inspect(
        MediaPipelineStagePlan& encoder, AVCodecContext& context,
        MediaRational frameRate, const std::string& backend,
        std::optional<AVPixelFormat> advertisedEquivalentSoftwareSurface);

private:
    MediaOpenedVideoEncoderProbe() = delete;
};

} // namespace media::ffmpeg::graph
