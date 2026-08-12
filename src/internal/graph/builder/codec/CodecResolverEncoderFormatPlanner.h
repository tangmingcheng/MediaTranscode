#pragma once

#include "internal/graph/core/MediaNodeOptions.h"
#include "media_transcode/Result.h"

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVCodec;

namespace media::ffmpeg::graph {

struct CodecResolverEncoderFormatPlanRequest {
    const AVCodec* encoder = nullptr;
    const MediaNodeOptions* options = nullptr;
};

struct CodecResolverEncoderFormatPlan {
    AVPixelFormat encoderPixelFormat = AV_PIX_FMT_NONE;
    AVPixelFormat hardwareFramesFormat = AV_PIX_FMT_NONE;
    AVPixelFormat surfaceSoftwareFormat = AV_PIX_FMT_NONE;
    bool requiresHardwareDeviceContext = false;
    bool requiresHardwareFramesContext = false;
};

class CodecResolverEncoderFormatPlanner final {
public:
    static ::media::Result<CodecResolverEncoderFormatPlan> build(
        const CodecResolverEncoderFormatPlanRequest& request);

private:
    CodecResolverEncoderFormatPlanner() = default;
};

} // namespace media::ffmpeg::graph
