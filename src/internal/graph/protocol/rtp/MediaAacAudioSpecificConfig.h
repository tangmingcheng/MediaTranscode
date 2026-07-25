#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaAacAudioSpecificConfig final {
    int audioObjectType = 0;
    int sampleRate = 0;
    int channels = 0;
    int frameSamples = 0;
};

::media::Result<MediaAacAudioSpecificConfig> parseAacAudioSpecificConfig(
    const std::vector<uint8_t>& bytes);

} // namespace media::ffmpeg::graph
