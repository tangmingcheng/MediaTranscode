#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <limits>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::int64_t UnixToNtpSeconds = 2'208'988'800LL;
constexpr std::uint64_t NtpFractionScale = std::uint64_t{1} << 32;

::media::Result<MediaNtpTimestamp> invalidNtp(const char* message)
{
    return ::media::Result<MediaNtpTimestamp>::failure(
        ::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaSharedNtpEpoch::MediaSharedNtpEpoch(
    MediaRunningTime masterAtCapture,
    std::int64_t unixNanosecondsAtCapture) noexcept
    : m_masterAtCapture(masterAtCapture),
      m_unixNanosecondsAtCapture(unixNanosecondsAtCapture)
{
}

::media::Result<MediaSharedNtpEpoch> MediaSharedNtpEpoch::create(
    MediaRunningTime masterAtCapture,
    std::chrono::nanoseconds unixTimeAtCapture) noexcept
{
    const std::int64_t unixNanoseconds = unixTimeAtCapture.count();
    auto mapped = fromUnixNanoseconds(unixNanoseconds);
    if (!mapped) {
        return ::media::Result<MediaSharedNtpEpoch>::failure(mapped.error());
    }
    return ::media::Result<MediaSharedNtpEpoch>::success(
        MediaSharedNtpEpoch(masterAtCapture, unixNanoseconds));
}

::media::Result<MediaNtpTimestamp> MediaSharedNtpEpoch::map(
    MediaRunningTime masterTime) const noexcept
{
    auto delta = masterTime.checkedSubtract(m_masterAtCapture);
    if (!delta) {
        return invalidNtp("Shared NTP epoch master-time subtraction overflow");
    }
    const std::int64_t offset = delta.value().nanoseconds();
    if ((offset > 0 &&
         m_unixNanosecondsAtCapture >
             std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 &&
         m_unixNanosecondsAtCapture <
             std::numeric_limits<std::int64_t>::min() - offset)) {
        return invalidNtp("Shared NTP epoch Unix-time mapping overflow");
    }
    return fromUnixNanoseconds(m_unixNanosecondsAtCapture + offset);
}

::media::Result<MediaNtpTimestamp> MediaSharedNtpEpoch::fromUnixNanoseconds(
    std::int64_t unixNanoseconds) noexcept
{
    std::int64_t unixSeconds = unixNanoseconds / NanosecondsPerSecond;
    std::int64_t remainder = unixNanoseconds % NanosecondsPerSecond;
    if (remainder < 0) {
        --unixSeconds;
        remainder += NanosecondsPerSecond;
    }
    if (unixSeconds < -UnixToNtpSeconds) {
        return invalidNtp("Shared NTP epoch precedes the NTP epoch");
    }
    const auto ntpSeconds = static_cast<std::uint64_t>(
        unixSeconds + UnixToNtpSeconds);
    const auto ntpFraction = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(remainder) * NtpFractionScale) /
        static_cast<std::uint64_t>(NanosecondsPerSecond));
    return ::media::Result<MediaNtpTimestamp>::success(
        MediaNtpTimestamp(ntpSeconds, ntpFraction));
}

} // namespace media::ffmpeg::graph
