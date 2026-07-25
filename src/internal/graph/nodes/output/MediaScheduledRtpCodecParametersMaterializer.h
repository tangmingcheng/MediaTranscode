#pragma once

#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaScheduledRtpCodecParametersMaterializer final {
public:
    static ::media::Result<::media::ffmpeg::CodecParametersPtr> materialize(
        const AVCodecContext& context,
        const MediaScheduledRtpPacketizationPlan& packetization);

private:
    MediaScheduledRtpCodecParametersMaterializer() = delete;
};

} // namespace media::ffmpeg::graph
