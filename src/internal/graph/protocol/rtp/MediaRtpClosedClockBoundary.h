#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace media::ffmpeg::graph {

// No clock observation after this capture cutoff can belong to the closed
// member. This bounds evaluation, not the original SR/CNAME observation time.
struct MediaRtpClosedClockMember final {
    std::uint64_t ingressGeneration;
    std::uint32_t ssrc;
    std::int64_t captureCutoffNs;

    std::int64_t evaluationTime(std::int64_t now) const noexcept
    { return std::min(now, captureCutoffNs); }
};

struct MediaRtpClosedClockBoundary final {
    std::uint64_t groupGeneration;
    MediaRtpClosedClockMember video;
    std::optional<MediaRtpClosedClockMember> audio;
};

} // namespace media::ffmpeg::graph
