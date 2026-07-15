#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMaterializedStreamConfig.h"

struct AVCodecParameters;

namespace media::ffmpeg::graph {

class MediaTsFfmpegStreamConfigMaterializer final {
public:
    static ::media::Result<MediaTsMaterializedVideoConfig> video(
        const MediaTsMuxPlan& plan,
        const AVCodecParameters& parameters);

    static ::media::Result<MediaTsMaterializedAudioConfig> audio(
        const MediaTsMuxPlan& plan,
        const AVCodecParameters& parameters);
};

} // namespace media::ffmpeg::graph
