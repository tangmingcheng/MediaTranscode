#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <span>
#include <vector>

namespace media_transcode::test {

struct MediaTsSampleAvccConfig final {
    std::uint8_t nalLengthBytes;
    std::vector<std::uint8_t> sequenceParameterSet;
    std::vector<std::uint8_t> pictureParameterSet;
};

class MediaTsSampleStreamConfigFixture final {
public:
    static ::media::Result<MediaTsSampleAvccConfig> parseAvcc(
        std::span<const std::uint8_t> extradata);
    static ::media::Result<std::uint8_t> aacSamplingFrequencyIndex(
        int sampleRate);
};

} // namespace media_transcode::test
