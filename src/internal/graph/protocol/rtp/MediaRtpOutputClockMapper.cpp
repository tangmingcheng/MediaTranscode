#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;

::media::Result<MediaRtpTimestamp> invalidTimestamp(const char* message)
{
    return ::media::Result<MediaRtpTimestamp>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaRtpOutputClockMapper::MediaRtpOutputClockMapper(
    int clockRate,
    std::uint32_t baseTimestamp,
    MediaRunningTime masterOrigin) noexcept
    : m_clockRate(clockRate),
      m_baseTimestamp(baseTimestamp),
      m_masterOrigin(masterOrigin)
{
}

::media::Result<MediaRtpOutputClockMapper>
MediaRtpOutputClockMapper::create(
    int clockRate,
    std::uint32_t baseTimestamp,
    MediaRunningTime masterOrigin) noexcept
{
    if (clockRate <= 0) {
        return ::media::Result<MediaRtpOutputClockMapper>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP output clock rate must be positive"));
    }
    return ::media::Result<MediaRtpOutputClockMapper>::success(
        MediaRtpOutputClockMapper(clockRate, baseTimestamp, masterOrigin));
}

::media::Result<MediaRtpTimestamp> MediaRtpOutputClockMapper::map(
    MediaRunningTime presentationOnMaster) const noexcept
{
    auto deltaResult = presentationOnMaster.checkedSubtract(m_masterOrigin);
    if (!deltaResult) {
        return invalidTimestamp("RTP output master-time subtraction overflow");
    }
    const std::int64_t delta = deltaResult.value().nanoseconds();
    if (delta < 0) {
        return invalidTimestamp(
            "RTP output presentation precedes the common master origin");
    }
    const std::int64_t seconds = delta / NanosecondsPerSecond;
    const std::int64_t remainder = delta % NanosecondsPerSecond;
    if (seconds > std::numeric_limits<std::int64_t>::max() / m_clockRate) {
        return invalidTimestamp("RTP output timestamp rescale overflow");
    }
    const std::int64_t wholeTicks = seconds * m_clockRate;
    const std::int64_t fractionalTicks =
        (remainder * static_cast<std::int64_t>(m_clockRate)) /
        NanosecondsPerSecond;
    if (wholeTicks >
        std::numeric_limits<std::int64_t>::max() - fractionalTicks) {
        return invalidTimestamp("RTP output timestamp rescale overflow");
    }
    const std::int64_t ticksFromOrigin = wholeTicks + fractionalTicks;
    const std::uint64_t extendedTicks =
        static_cast<std::uint64_t>(m_baseTimestamp) +
        static_cast<std::uint64_t>(ticksFromOrigin);
    const std::uint32_t wire = static_cast<std::uint32_t>(extendedTicks);
    return ::media::Result<MediaRtpTimestamp>::success(
        MediaRtpTimestamp(extendedTicks, wire));
}

} // namespace media::ffmpeg::graph
