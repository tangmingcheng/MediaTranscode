#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include <cmath>
#include <limits>

namespace media::ffmpeg::graph {

// Immutable affine clock facts captured with the input AU. The calibrated
// slope is independent of the codec's nominal timestamp time base.
struct MediaSourceClockProjection final {
    std::int64_t tickAnchor;
    MediaRunningTime sourceAnchor;
    std::int64_t rateNumeratorNs;
    std::int64_t rateDenominatorTicks;
    MediaRunningTime commonEpoch;

    ::media::Result<MediaRunningTime> project(std::int64_t ticks) const
    {
        using R = ::media::Result<MediaRunningTime>;
        if (rateNumeratorNs <= 0 || rateDenominatorTicks <= 0)
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Source clock projection requires a positive calibrated rate"));
        // Subtract in integer space so adjacent large extended timestamps
        // remain distinct on MSVC, where long double is binary64.
        if ((tickAnchor > 0 && ticks < (std::numeric_limits<std::int64_t>::min)() + tickAnchor) ||
            (tickAnchor < 0 && ticks > (std::numeric_limits<std::int64_t>::max)() + tickAnchor))
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Source clock timestamp delta is not representable"));
        const long double delta = static_cast<long double>(ticks - tickAnchor);
        // Cancel the absolute NTP epoch before floating-point arithmetic.
        // Otherwise binary64 loses sub-microsecond relative times at NTP scale.
        auto relativeAnchor = sourceAnchor.checkedSubtract(commonEpoch);
        if (!relativeAnchor) return R::failure(relativeAnchor.error());
        const long double normalized = relativeAnchor.value().nanoseconds() +
            delta * rateNumeratorNs / rateDenominatorTicks;
        const long double lower = static_cast<long double>(
            (std::numeric_limits<std::int64_t>::min)());
        // The exclusive upper limit is representable even when long double
        // is binary64 (MSVC); INT64_MAX rounds up on that platform.
        if (!std::isfinite(normalized) || normalized < lower || normalized >= -lower)
            return R::failure(::media::ErrorInfo::invalidArgument(
                "Source clock projection is not representable"));
        return R::success(MediaRunningTime::fromNanoseconds(
            static_cast<std::int64_t>(normalized)));
    }
};

} // namespace media::ffmpeg::graph
