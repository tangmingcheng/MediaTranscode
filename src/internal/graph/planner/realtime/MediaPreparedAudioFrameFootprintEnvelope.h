#pragma once

#include <cstdint>
#include <string>

namespace media::ffmpeg::graph {

struct MediaPreparedAudioFrameFootprintEnvelope final {
    std::uint64_t decodedFrameBytes = 0;
    std::uint64_t resampledFrameBytes = 0;
    std::uint64_t encoderFrameBytes = 0;
    std::uint64_t maximumFrameBytes = 0;
    std::string authority;

    bool valid() const noexcept
    {
        return decodedFrameBytes > 0 && resampledFrameBytes > 0 &&
            encoderFrameBytes > 0 && maximumFrameBytes >= decodedFrameBytes &&
            maximumFrameBytes >= resampledFrameBytes &&
            maximumFrameBytes >= encoderFrameBytes && !authority.empty();
    }
};

} // namespace media::ffmpeg::graph
