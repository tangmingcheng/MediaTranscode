#pragma once
#include <cstdint>
namespace media::ffmpeg::graph {
struct MediaVideoOutputReadyEvidence final {
    std::uint64_t generation;
    std::uint64_t finalGlobalSequence;
    std::int64_t submittedAtNanoseconds;
};
} // namespace media::ffmpeg::graph
