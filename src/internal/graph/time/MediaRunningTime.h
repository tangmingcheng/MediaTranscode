#pragma once

#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include "media_transcode/Result.h"

#include <compare>
#include <cstdint>
#include <limits>
#include <string>

namespace media::ffmpeg::graph {

class MediaRunningTime final {
public:
    static constexpr MediaRunningTime fromNanoseconds(std::int64_t nanoseconds) noexcept
    {
        return MediaRunningTime(nanoseconds);
    }

    static ::media::Result<MediaRunningTime> checkedFromTicks(
        std::int64_t ticks,
        int timeBaseNumerator,
        int timeBaseDenominator) noexcept
    {
        if (timeBaseNumerator <= 0 || timeBaseDenominator <= 0) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaRunningTime requires a positive time base"));
        }

        const std::uint64_t magnitude = ticks >= 0
            ? static_cast<std::uint64_t>(ticks)
            : static_cast<std::uint64_t>(-(ticks + 1)) + 1;
        const std::uint64_t maximumMagnitude = ticks >= 0
            ? static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            : std::uint64_t{1} << 63;
        const std::uint64_t nanosecondsPerSecond = 1'000'000'000;
        const std::uint64_t scale =
            static_cast<std::uint64_t>(timeBaseNumerator) * nanosecondsPerSecond;
        const MediaUInt128 scaledTicks =
            MediaCheckedArithmetic::multiplyWide(magnitude, scale);
        const MediaUInt128 firstOverflowValue =
            MediaCheckedArithmetic::multiplyWide(
            maximumMagnitude + 1,
            static_cast<std::uint64_t>(timeBaseDenominator));
        if (scaledTicks >= firstOverflowValue) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaRunningTime tick rescale is not representable"));
        }

        const std::uint64_t rescaledMagnitude =
            MediaCheckedArithmetic::divideWideBy32(
            scaledTicks,
            static_cast<std::uint32_t>(timeBaseDenominator));
        const std::int64_t nanoseconds = ticks >= 0
            ? static_cast<std::int64_t>(rescaledMagnitude)
            : rescaledMagnitude == (std::uint64_t{1} << 63)
                ? std::numeric_limits<std::int64_t>::min()
                : -static_cast<std::int64_t>(rescaledMagnitude);
        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime(nanoseconds));
    }

    constexpr std::int64_t nanoseconds() const noexcept
    {
        return m_nanoseconds;
    }

    ::media::Result<MediaRunningTime> checkedAdd(MediaRunningTime other) const noexcept
    {
        if ((other.m_nanoseconds > 0 &&
             m_nanoseconds > std::numeric_limits<std::int64_t>::max() - other.m_nanoseconds) ||
            (other.m_nanoseconds < 0 &&
             m_nanoseconds < std::numeric_limits<std::int64_t>::min() - other.m_nanoseconds)) {
            return overflowFailure("addition");
        }

        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime(m_nanoseconds + other.m_nanoseconds));
    }

    ::media::Result<MediaRunningTime> checkedSubtract(MediaRunningTime other) const noexcept
    {
        if ((other.m_nanoseconds > 0 &&
             m_nanoseconds < std::numeric_limits<std::int64_t>::min() + other.m_nanoseconds) ||
            (other.m_nanoseconds < 0 &&
             m_nanoseconds > std::numeric_limits<std::int64_t>::max() + other.m_nanoseconds)) {
            return overflowFailure("subtraction");
        }

        return ::media::Result<MediaRunningTime>::success(
            MediaRunningTime(m_nanoseconds - other.m_nanoseconds));
    }

    friend constexpr bool operator==(MediaRunningTime, MediaRunningTime) noexcept = default;
    friend constexpr auto operator<=>(MediaRunningTime, MediaRunningTime) noexcept = default;

private:
    explicit constexpr MediaRunningTime(std::int64_t nanoseconds) noexcept
        : m_nanoseconds(nanoseconds)
    {
    }

    static ::media::Result<MediaRunningTime> overflowFailure(const char* operation)
    {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument(
                std::string("MediaRunningTime ") + operation + " overflow"));
    }

    std::int64_t m_nanoseconds = 0;
};

} // namespace media::ffmpeg::graph
