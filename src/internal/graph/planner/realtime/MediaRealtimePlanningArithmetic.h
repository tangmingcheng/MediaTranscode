#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaRealtimePlanningArithmetic final {
public:
    static ::media::Result<std::uint64_t> add(
        std::uint64_t left, std::uint64_t right, const char* fact);
    static ::media::Result<std::uint64_t> multiply(
        std::uint64_t left, std::uint64_t right, const char* fact);
    static ::media::Result<std::uint64_t> ceilScale(
        std::uint64_t value,
        std::uint64_t numerator,
        std::uint64_t denominator,
        const char* fact);
    static ::media::Result<std::uint64_t> bytesForResidence(
        std::uint64_t rate,
        MediaRunningTime residence,
        const char* fact);

private:
    MediaRealtimePlanningArithmetic() = delete;
};

} // namespace media::ffmpeg::graph
