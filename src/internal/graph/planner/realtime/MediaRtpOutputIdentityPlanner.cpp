#include "internal/graph/planner/realtime/MediaRtpOutputIdentityPlanner.h"

namespace media::ffmpeg::graph {

std::uint32_t MediaRtpOutputIdentityPlanner::stableNumeric(
    std::string_view identity) noexcept
{
    std::uint32_t hash = 2166136261u;
    for (const unsigned char byte : identity) {
        hash = (hash ^ byte) * 16777619u;
    }
    return hash == 0 ? 1u : hash;
}

std::uint32_t MediaRtpOutputIdentityPlanner::stableFfmpegMuxSsrc(
    std::string_view identity) noexcept
{
    const std::uint32_t constrained = stableNumeric(identity) & 0x7fffffffu;
    return constrained == 0 ? 1u : constrained;
}

std::uint16_t MediaRtpOutputIdentityPlanner::stableSequenceNumber(
    std::string_view identity) noexcept
{
    return static_cast<std::uint16_t>(stableNumeric(identity));
}

std::string MediaRtpOutputIdentityPlanner::cname(
    std::string_view sessionIdentity)
{
    return "mt-" + std::to_string(stableNumeric(sessionIdentity)) +
        "@media-transcode";
}

} // namespace media::ffmpeg::graph
