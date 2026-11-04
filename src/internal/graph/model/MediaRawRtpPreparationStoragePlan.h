#pragma once

#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaPreparedRtpDatagramAllocationFacts final {
    std::size_t bytes;
    std::size_t alignment;
};

struct MediaRawRtpPreparationStoragePlan final {
    std::size_t maximumObservedWireBytes;
    std::size_t maximumDatagramsPerStream;
    std::size_t streamCount;
    std::size_t maximumAllocationBytes;
    MediaPreparedRtpDatagramAllocationFacts entryAllocation;
};

} // namespace media::ffmpeg::graph
