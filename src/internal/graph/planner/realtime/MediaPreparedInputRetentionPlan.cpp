#include "internal/graph/planner/realtime/MediaPreparedInputRetentionPlan.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
bool MediaPreparedInputRetentionStream::valid() const noexcept
{
    return maximumUnits > 0 &&
        maximumUnits <= static_cast<std::size_t>((std::numeric_limits<int>::max)()) &&
        maximumUnitBytes > 0 &&
        maximumUnitBytes <= (std::numeric_limits<std::uint64_t>::max)() / maximumUnits &&
        maximumBytes == maximumUnitBytes * maximumUnits &&
        maximumBytes >= maximumUnitBytes &&
        maximumBytes <= static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) &&
        !authority.empty();
}

::media::Status MediaPreparedInputRetentionPlan::validate() const
{
    if (acquisitionWindow.nanoseconds() <= 0 || !video.valid() || (audio && !audio->valid())) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "prepared input retention requires bounded acquisition products for every present stream"));
    }
    return ::media::Status::success();
}

::media::Result<MediaPreparedInputRetentionStream>
MediaPreparedInputRetentionPlanner::planStream(
    MediaRunningTime acquisitionWindow,
    MediaRational observedAccessUnitRate,
    std::uint64_t replayAccessUnitBound,
    const MediaPreparedInputPayloadBound& payload,
    std::string cadenceAuthority)
{
    using Result = ::media::Result<MediaPreparedInputRetentionStream>;
    if (acquisitionWindow.nanoseconds() <= 0 ||
        observedAccessUnitRate.num <= 0 || observedAccessUnitRate.den <= 0 ||
        !payload.valid() || cadenceAuthority.empty()) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "input retention requires source cadence, payload and acquisition facts"));
    }
    auto denominator = MediaCheckedArithmetic::multiply(
        static_cast<std::uint64_t>(observedAccessUnitRate.den),
        1'000'000'000ULL, "input retention cadence denominator");
    auto windowUnits = denominator ? MediaCheckedArithmetic::ceilScale(
        static_cast<std::uint64_t>(acquisitionWindow.nanoseconds()),
        static_cast<std::uint64_t>(observedAccessUnitRate.num),
        denominator.value(), "input acquisition retention units") : denominator;
    auto units = windowUnits ? MediaCheckedArithmetic::add(
        windowUnits.value(), replayAccessUnitBound,
        "input acquisition and sealed replay units") : windowUnits;
    if (!units) return Result::failure(units.error());
    auto bytes = MediaCheckedArithmetic::multiply(
        units.value(), payload.maximumPayloadBytes, "input retention payload bytes");
    if (!bytes) return Result::failure(bytes.error());
    if (units.value() > static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "input retention exceeds serialized node capacity range"));
    }
    MediaPreparedInputRetentionStream result{
        static_cast<std::size_t>(units.value()), bytes.value(),
        payload.maximumPayloadBytes,
        std::move(cadenceAuthority) + "+acquisition-window+sealed-replay+" + payload.authority};
    if (!result.valid()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "input retention exceeds serialized payload capacity range"));
    return Result::success(std::move(result));
}

} // namespace media::ffmpeg::graph
