#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaAvReacquisitionReason {
    FutureGeneration,
    HardDiscontinuity,
    Flush
};

struct MediaAvReacquisitionRequest final {
    std::uint64_t observedGeneration;
    MediaAvReacquisitionReason reason;

    bool operator==(const MediaAvReacquisitionRequest&) const = default;
};

} // namespace media::ffmpeg::graph
