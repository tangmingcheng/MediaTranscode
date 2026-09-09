#pragma once

#include <string>

namespace media::ffmpeg::graph {

// Immutable readback from the opened decoder, captured before worker ownership.
struct MediaDecoderRuntimeFacts final {
    std::string decoderName;
    int hardwareAccelerationFlags;
};

} // namespace media::ffmpeg::graph
