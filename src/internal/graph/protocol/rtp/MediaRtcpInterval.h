#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"
#include <cstdint>
#include <limits>

namespace media::ffmpeg::graph {

// RFC 3550 section 6.3.1: [0.5, 1.5] randomization / 1.21828.
// Both admission and runtime use this exact floor-rounded integer mapping.
inline constexpr std::uint64_t MediaRtcpRandomFactorMinimum = 500000;
inline constexpr std::uint64_t MediaRtcpRandomFactorRange = 1000001;
inline constexpr std::uint64_t MediaRtcpCompensationDivisor = 1218280;

inline ::media::Result<MediaRunningTime> mediaRtcpIntervalForFactor(
    MediaRunningTime base,
    std::uint64_t factor) noexcept
{
    if (base.nanoseconds() <= 0 || factor < MediaRtcpRandomFactorMinimum ||
        factor >= MediaRtcpRandomFactorMinimum + MediaRtcpRandomFactorRange) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP random interval requires a positive base and RFC random factor"));
    }
    const auto baseNs = static_cast<std::uint64_t>(base.nanoseconds());
    const auto quotient = baseNs / MediaRtcpCompensationDivisor;
    const auto remainder = baseNs % MediaRtcpCompensationDivisor;
    if (quotient > static_cast<std::uint64_t>(
                       std::numeric_limits<std::int64_t>::max()) /
                       factor) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP randomized interval overflow"));
    }
    const auto whole = quotient * factor;
    const auto fractional = remainder * factor / MediaRtcpCompensationDivisor;
    if (whole > static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) - fractional) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP randomized interval overflow"));
    }
    const auto intervalNs = whole + fractional;
    if (intervalNs == 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP randomized interval rounded to zero"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(intervalNs)));
}

} // namespace media::ffmpeg::graph
