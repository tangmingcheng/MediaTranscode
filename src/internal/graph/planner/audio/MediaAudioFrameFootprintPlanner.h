#pragma once

#include "internal/graph/planner/MediaAudioPipelinePlanner.h"

namespace media::ffmpeg::graph {

struct MediaAudioSourceFrameFootprints final {
    std::uint64_t decodedBytes;
    std::uint64_t resampledBytes;
};

class MediaAudioFrameFootprintPlanner final {
public:
    static ::media::Result<std::uint64_t> logicalBytes(
        std::int64_t samples, int channels, const std::string& sampleFormat,
        const char* fact);
    static ::media::Result<MediaAudioSourceFrameFootprints> planSource(
        const MediaAudioPipelinePlan& audio, std::int64_t maximumResamplerOutputSamples);
};

} // namespace media::ffmpeg::graph
