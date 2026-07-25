#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"

#include <algorithm>
#include <array>
#include <limits>

namespace media::ffmpeg::graph {

namespace {

constexpr std::int64_t ptsModulus = std::int64_t{1} << 33;

std::uint64_t distance(std::int64_t lhs, std::int64_t rhs) noexcept
{
    const auto magnitude = [](std::int64_t value) noexcept {
        return value >= 0
            ? static_cast<std::uint64_t>(value)
            : static_cast<std::uint64_t>(-(value + 1)) + 1;
    };
    if ((lhs < 0) != (rhs < 0)) return magnitude(lhs) + magnitude(rhs);
    const std::uint64_t left = magnitude(lhs);
    const std::uint64_t right = magnitude(rhs);
    return left >= right ? left - right : right - left;
}

::media::Result<std::int64_t> alignToNearestEpoch(std::int64_t raw,
                                                  std::int64_t anchor)
{
    if (anchor < 0 || raw < 0 || raw >= ptsModulus) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS epoch alignment input is invalid"));
    }

    const std::int64_t anchorEpoch = anchor / ptsModulus;
    std::array<std::int64_t, 3> candidates{};
    std::size_t count = 0;
    auto appendCandidate = [&](std::int64_t epoch) {
        if (epoch == -1) {
            candidates[count++] = raw - ptsModulus;
            return;
        }
        if (epoch < 0 || epoch >
            (std::numeric_limits<std::int64_t>::max() - raw) / ptsModulus) return;
        candidates[count++] = epoch * ptsModulus + raw;
    };
    appendCandidate(anchorEpoch - 1);
    appendCandidate(anchorEpoch);
    if (anchorEpoch < std::numeric_limits<std::int64_t>::max() / ptsModulus)
        appendCandidate(anchorEpoch + 1);
    if (count == 0) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS epoch has no representable candidate"));
    }

    std::int64_t selected = candidates[0];
    std::uint64_t selectedDistance = distance(selected, anchor);
    for (std::size_t index = 1; index < count; ++index) {
        const std::uint64_t candidateDistance = distance(candidates[index], anchor);
        if (candidateDistance < selectedDistance ||
            (candidateDistance == selectedDistance && candidates[index] < selected)) {
            selected = candidates[index];
            selectedDistance = candidateDistance;
        }
    }
    return ::media::Result<std::int64_t>::success(selected);
}

::media::Result<std::int64_t> checkedAdd(std::int64_t lhs, std::int64_t rhs)
{
    if ((rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
        (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS extended timestamp overflow"));
    }
    return ::media::Result<std::int64_t>::success(lhs + rhs);
}

::media::Result<std::int64_t> checkedSubtract(std::int64_t lhs, std::int64_t rhs)
{
    if ((rhs > 0 && lhs < std::numeric_limits<std::int64_t>::min() + rhs) ||
        (rhs < 0 && lhs > std::numeric_limits<std::int64_t>::max() + rhs)) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS source delta overflow"));
    }
    return ::media::Result<std::int64_t>::success(lhs - rhs);
}

} // namespace

MediaTsSourceClockMapper::MediaTsSourceClockMapper(
    MediaTsPcrCalibration calibration,
    MediaTimestampUnwrapper ptsUnwrapper,
    MediaTimestampUnwrapper dtsUnwrapper) noexcept
    : m_calibration(calibration)
    , m_ptsUnwrapper(std::move(ptsUnwrapper))
    , m_dtsUnwrapper(std::move(dtsUnwrapper))
{
}

::media::Result<MediaTsSourceClockMapper> MediaTsSourceClockMapper::create(
    std::optional<MediaTsPcrCalibration> calibration)
{
    if (!calibration || calibration->pcr27Mhz < 0) {
        return ::media::Result<MediaTsSourceClockMapper>::failure(
            ::media::ErrorInfo::notInitialized("Locked non-negative PCR evidence is required for source mapping"));
    }
    auto pts = MediaTimestampUnwrapper::create(
        MediaTimestampCounterKind::MpegTsPtsDts33, calibration->generation);
    auto dts = MediaTimestampUnwrapper::create(
        MediaTimestampCounterKind::MpegTsPtsDts33, calibration->generation);
    if (!pts) return ::media::Result<MediaTsSourceClockMapper>::failure(pts.error());
    if (!dts) return ::media::Result<MediaTsSourceClockMapper>::failure(dts.error());
    return ::media::Result<MediaTsSourceClockMapper>::success(
        MediaTsSourceClockMapper(*calibration, std::move(pts.value()), std::move(dts.value())));
}

::media::Result<MediaTsMappedSourceTiming> MediaTsSourceClockMapper::map(
    std::optional<std::uint64_t> pts33,
    std::optional<std::uint64_t> dts33)
{
    MediaTsSourceClockMapper candidate = *this;
    auto pts = candidate.mapOne(
        pts33, candidate.m_ptsUnwrapper, candidate.m_ptsEpochOffset);
    if (!pts) return ::media::Result<MediaTsMappedSourceTiming>::failure(pts.error());
    auto dts = candidate.mapOne(
        dts33, candidate.m_dtsUnwrapper, candidate.m_dtsEpochOffset);
    if (!dts) return ::media::Result<MediaTsMappedSourceTiming>::failure(dts.error());
    MediaTsMappedSourceTiming result{pts.value(), dts.value(), m_calibration.generation};
    *this = std::move(candidate);
    return ::media::Result<MediaTsMappedSourceTiming>::success(std::move(result));
}

::media::Result<std::optional<MediaRunningTime>> MediaTsSourceClockMapper::mapOne(
    std::optional<std::uint64_t> raw,
    MediaTimestampUnwrapper& unwrapper,
    std::optional<std::int64_t>& epochOffset)
{
    if (!raw) {
        return ::media::Result<std::optional<MediaRunningTime>>::success(std::nullopt);
    }
    if (*raw >= static_cast<std::uint64_t>(ptsModulus)) {
        return ::media::Result<std::optional<MediaRunningTime>>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PTS/DTS is outside 33-bit range"));
    }
    auto timestamp = MediaProtocolTimestamp::create(static_cast<std::int64_t>(*raw), 1, 90'000);
    if (!timestamp) return ::media::Result<std::optional<MediaRunningTime>>::failure(timestamp.error());
    auto unwrapped = unwrapper.unwrap(timestamp.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        return ::media::Result<std::optional<MediaRunningTime>>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PTS/DTS regression or unwrap failure"));
    }

    const std::int64_t pcrAnchor90Khz = m_calibration.pcr27Mhz / 300;
    std::int64_t aligned = 0;
    if (!epochOffset) {
        auto initial = alignToNearestEpoch(unwrapped.timestamp->ticks(), pcrAnchor90Khz);
        if (!initial) {
            return ::media::Result<std::optional<MediaRunningTime>>::failure(initial.error());
        }
        aligned = initial.value();
        epochOffset = aligned - unwrapped.timestamp->ticks();
    } else {
        auto extended = checkedAdd(unwrapped.timestamp->ticks(), *epochOffset);
        if (!extended) {
            return ::media::Result<std::optional<MediaRunningTime>>::failure(extended.error());
        }
        aligned = extended.value();
    }
    auto deltaTicks = checkedSubtract(aligned, pcrAnchor90Khz);
    if (!deltaTicks) {
        return ::media::Result<std::optional<MediaRunningTime>>::failure(deltaTicks.error());
    }
    auto delta = MediaRunningTime::checkedFromTicks(
        deltaTicks.value(), 1, 90'000);
    if (!delta) return ::media::Result<std::optional<MediaRunningTime>>::failure(delta.error());
    auto sourceTime = m_calibration.sourceTime.checkedAdd(delta.value());
    if (!sourceTime) return ::media::Result<std::optional<MediaRunningTime>>::failure(sourceTime.error());
    return ::media::Result<std::optional<MediaRunningTime>>::success(sourceTime.value());
}

} // namespace media::ffmpeg::graph
