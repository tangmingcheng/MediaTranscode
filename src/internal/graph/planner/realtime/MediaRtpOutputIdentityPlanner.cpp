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
