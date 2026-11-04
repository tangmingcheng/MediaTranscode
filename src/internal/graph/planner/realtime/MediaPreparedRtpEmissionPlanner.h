#pragma once
#include "internal/graph/planner/MediaPreparedEncoderEmissionEnvelope.h"
#include "internal/graph/planner/MediaPreparedAudioEncoderEmissionEnvelope.h"
#include "internal/graph/protocol/rtp/MediaRtpAccessUnitEmissionContract.h"
#include "internal/graph/runtime/buffer/FFmpegCodecParametersSnapshot.h"
#include "internal/graph/planner/realtime/MediaRawRtpProducerPlan.h"
#include <vector>
#include <string_view>

namespace media::ffmpeg::graph {
struct MediaPreparedRealtimeEmissionSet;
class MediaPreparedRtpEmissionPlanner final {
public:
    static ::media::Result<std::vector<MediaRawRtpProducerPlan>> producers(
        MediaDatagramProtocolLayout layout, const MediaPreparedRealtimeEmissionSet& emission,
        std::size_t maximumDatagramBytes, std::string_view cname);
    static ::media::Result<MediaRtpAccessUnitEmissionContract> video(
        const MediaPreparedEncoderEmissionEnvelope& emission,
        const FFmpegCodecParametersSnapshot& parameters,
        std::size_t maximumDatagramBytes);
    static ::media::Result<MediaRtpAccessUnitEmissionContract> audio(
        const MediaPreparedAudioEncoderEmissionEnvelope& emission,
        std::size_t maximumDatagramBytes);
};
} // namespace media::ffmpeg::graph
