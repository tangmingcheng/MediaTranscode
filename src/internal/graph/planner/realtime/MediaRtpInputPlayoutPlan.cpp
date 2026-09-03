#include "internal/graph/planner/realtime/MediaRtpInputPlayoutPlan.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <limits>

namespace media::ffmpeg::graph {

::media::Status MediaRtpInputPlayoutPlan::validate() const
{
    if (latency.nanoseconds() <= 0 ||
        startupAccessUnits == 0 ||
        maximumRetainedAccessUnits == 0 ||
        startupAccessUnits >= maximumRetainedAccessUnits ||
        maximumRetainedPayloadBytes == 0 || authority.empty()) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "RTP input playout plan is incomplete"));
    }
    return ::media::Status::success();
}

::media::Result<MediaRtpInputPlayoutPlan> MediaRtpInputPlayoutPlanner::plan(
    MediaRunningTime latency,
    MediaRational accessUnitRate,
    const MediaPreparedRtpAccessUnitEnvelope& accessUnitEnvelope)
{
    using Result = ::media::Result<MediaRtpInputPlayoutPlan>;
    if (latency.nanoseconds() <= 0 || !accessUnitRate.isKnown() ||
        accessUnitRate.num <= 0 || accessUnitRate.den <= 0) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP input playout requires positive latency and access-unit rate facts"));
    }
    if (auto status = accessUnitEnvelope.validate(); !status) {
        return Result::failure(status.error());
    }
    auto denominator = MediaCheckedArithmetic::multiply(
        1'000'000'000ULL, static_cast<std::uint64_t>(accessUnitRate.den),
        "RTP input playout access-unit denominator");
    auto windowUnits = denominator
        ? MediaCheckedArithmetic::ceilScale(
              static_cast<std::uint64_t>(latency.nanoseconds()),
              static_cast<std::uint64_t>(accessUnitRate.num),
              denominator.value(),
              "RTP input playout access-unit window")
        : ::media::Result<std::uint64_t>::failure(denominator.error());
    auto retainedUnits = windowUnits
        ? MediaCheckedArithmetic::add(
              windowUnits.value(),
              accessUnitEnvelope.maximumAccessUnitsPerPush,
              "RTP input playout retained access units")
        : windowUnits;
    auto retainedBytes = retainedUnits
        ? MediaCheckedArithmetic::multiply(
              retainedUnits.value(),
              accessUnitEnvelope.maximumAccessUnitBytes,
              "RTP input playout retained payload bytes")
        : retainedUnits;
    if (!denominator || !windowUnits || !retainedUnits || !retainedBytes) {
        return Result::failure(
            !denominator ? denominator.error() :
            !windowUnits ? windowUnits.error() :
            !retainedUnits ? retainedUnits.error() : retainedBytes.error());
    }
    if (retainedUnits.value() == 0 ||
        retainedUnits.value() > (std::numeric_limits<std::size_t>::max)()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP input playout retained access units exceed runtime range"));
    }
    MediaRtpInputPlayoutPlan result{
        latency,
        static_cast<std::size_t>(windowUnits.value()),
        static_cast<std::size_t>(retainedUnits.value()),
        retainedBytes.value(),
        "GStreamer-rtpjitterbuffer-faststart-consecutive-access-units+RTP-timestamp-playout+prepared-source-access-unit-rate+prepared-access-unit-envelope"};
    if (auto status = result.validate(); !status) {
        return Result::failure(status.error());
    }
    return Result::success(std::move(result));
}

} // namespace media::ffmpeg::graph
