#include "internal/graph/protocol/rtp/MediaRtpPacketTimestampAligner.h"

#include <limits>

namespace media::ffmpeg::graph {

::media::Result<std::uint64_t> MediaRtpPacketTimestampAligner::align(
    const MediaRtpSourceClockCalibration& calibration,
    std::uint32_t rawRtpTimestamp) const
{
    constexpr std::uint32_t HalfRange = 0x80000000U;
    constexpr std::uint64_t FullRange = 0x100000000ULL;
    if (calibration.confidence != MediaRtpSourceClockConfidence::Locked ||
        calibration.extendedRtpAnchor < 0 ||
        static_cast<std::uint32_t>(calibration.extendedRtpAnchor) !=
            calibration.rtpAnchor) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet timestamp alignment requires a coherent locked anchor"));
    }
    const std::uint32_t modularDelta = rawRtpTimestamp - calibration.rtpAnchor;
    if (modularDelta == HalfRange) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet timestamp is half-range ambiguous"));
    }
    const std::int64_t signedDelta = modularDelta < HalfRange
        ? static_cast<std::int64_t>(modularDelta)
        : static_cast<std::int64_t>(static_cast<std::uint64_t>(modularDelta) -
                                    FullRange);
    if ((signedDelta > 0 && calibration.extendedRtpAnchor >
                               std::numeric_limits<std::int64_t>::max() - signedDelta) ||
        (signedDelta < 0 && calibration.extendedRtpAnchor < -signedDelta)) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP packet timestamp alignment overflows the locked cycle"));
    }
    return ::media::Result<std::uint64_t>::success(
        static_cast<std::uint64_t>(calibration.extendedRtpAnchor + signedDelta));
}

} // namespace media::ffmpeg::graph
