#include "internal/graph/protocol/mpegts/MediaTsSourceClockMapper.h"

#include <limits>

namespace media::ffmpeg::graph {

namespace {
constexpr std::int64_t ptsModulus = std::int64_t{1} << 33;

::media::Result<std::int64_t> alignToNearestEpoch(std::int64_t raw,
                                                  std::int64_t anchor)
{
    if (anchor < 0 || raw < 0) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS clock anchors must be non-negative"));
    }
    const std::int64_t difference = anchor - raw;
    std::int64_t epoch = difference / ptsModulus;
    const std::int64_t remainder = difference % ptsModulus;
    if (remainder >= ptsModulus / 2) ++epoch;

    if (epoch > 0 && epoch >
        (std::numeric_limits<std::int64_t>::max() - raw) / ptsModulus) {
        return ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS source epoch alignment overflow"));
    }
    return ::media::Result<std::int64_t>::success(raw + epoch * ptsModulus);
}
}

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
    if (!calibration) {
        return ::media::Result<MediaTsSourceClockMapper>::failure(
            ::media::ErrorInfo::notInitialized("Locked PCR evidence is required for source mapping"));
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
    auto pts = mapOne(pts33, m_ptsUnwrapper);
    if (!pts) return ::media::Result<MediaTsMappedSourceTiming>::failure(pts.error());
    auto dts = mapOne(dts33, m_dtsUnwrapper);
    if (!dts) return ::media::Result<MediaTsMappedSourceTiming>::failure(dts.error());
    return ::media::Result<MediaTsMappedSourceTiming>::success(
        MediaTsMappedSourceTiming{pts.value(), dts.value(), m_calibration.generation});
}

::media::Result<std::optional<MediaRunningTime>> MediaTsSourceClockMapper::mapOne(
    std::optional<std::uint64_t> raw,
    MediaTimestampUnwrapper& unwrapper)
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
    auto candidateUnwrapper = unwrapper;
    auto unwrapped = candidateUnwrapper.unwrap(timestamp.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        return ::media::Result<std::optional<MediaRunningTime>>::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PTS/DTS regression or unwrap failure"));
    }

    const std::int64_t pcrAnchor90Khz = m_calibration.pcr27Mhz / 300;
    auto aligned = alignToNearestEpoch(unwrapped.timestamp->ticks(), pcrAnchor90Khz);
    if (!aligned) {
        return ::media::Result<std::optional<MediaRunningTime>>::failure(aligned.error());
    }
    auto delta = MediaRunningTime::checkedFromTicks(aligned.value() - pcrAnchor90Khz, 1, 90'000);
    if (!delta) return ::media::Result<std::optional<MediaRunningTime>>::failure(delta.error());
    auto sourceTime = m_calibration.sourceTime.checkedAdd(delta.value());
    if (!sourceTime) return ::media::Result<std::optional<MediaRunningTime>>::failure(sourceTime.error());
    unwrapper = std::move(candidateUnwrapper);
    return ::media::Result<std::optional<MediaRunningTime>>::success(sourceTime.value());
}

} // namespace media::ffmpeg::graph
