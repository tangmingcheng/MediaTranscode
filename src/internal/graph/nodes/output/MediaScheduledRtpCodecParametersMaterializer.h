#pragma once

#include "internal/graph/model/MediaGraphTypes.h"

#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "media_transcode/Result.h"

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class MediaScheduledRtpCodecParametersMaterializer final {
public:
    static ::media::Result<::media::ffmpeg::CodecParametersPtr> materialize(
        const AVCodecParameters& context,
        MediaRational timeBase,
        const MediaScheduledRtpPacketizationPlan& packetization);

private:
    MediaScheduledRtpCodecParametersMaterializer() = delete;
};

} // namespace media::ffmpeg::graph
