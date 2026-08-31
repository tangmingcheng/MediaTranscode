#include "internal/graph/planner/realtime/MediaDatagramPacingRatePlanner.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <algorithm>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint64_t WebRtcNoFeedbackPacingNumerator = 5;
constexpr std::uint64_t WebRtcNoFeedbackPacingDenominator = 2;

} // namespace

::media::Result<std::uint64_t>
MediaDatagramPacingRatePlanner::requiredWireBytesPerSecond(
    const MediaWireTrafficEnvelope& wire,
    MediaRunningTime maximumResidence)
{
    constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
    if (wire.sustainedWireBytesPerSecond == 0 ||
        wire.peakWireBytesPerSecond < wire.sustainedWireBytesPerSecond ||
        wire.burstWireBytes == 0 ||
        maximumResidence <= MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<std::uint64_t>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Datagram pacing-rate planning requires complete wire demand and a positive immutable residence"));
    }
    auto burstDrainRate = MediaCheckedArithmetic::ceilScale(
        wire.burstWireBytes, NanosecondsPerSecond,
        static_cast<std::uint64_t>(maximumResidence.nanoseconds()),
        "Datagram immutable-residence burst drain rate");
    if (!burstDrainRate) return burstDrainRate;
    auto noFeedbackPacingRate = MediaCheckedArithmetic::ceilScale(
        wire.sustainedWireBytesPerSecond,
        WebRtcNoFeedbackPacingNumerator,
        WebRtcNoFeedbackPacingDenominator,
        "WebRTC no-feedback default pacing rate");
    if (!noFeedbackPacingRate) return noFeedbackPacingRate;
    return ::media::Result<std::uint64_t>::success((std::max)({
        wire.peakWireBytesPerSecond,
        burstDrainRate.value(),
        noFeedbackPacingRate.value()}));
}

} // namespace media::ffmpeg::graph
