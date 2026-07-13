#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

struct MediaAvStartupSelectionWork final {
    std::uint64_t indexedUnits = 0;
    std::uint64_t coverageOperations = 0;
    std::uint64_t orderedIndexMutations = 0;
    std::uint64_t candidateOperations = 0;
    std::uint64_t releaseUnitsVisited = 0;
};

} // namespace media::ffmpeg::graph
