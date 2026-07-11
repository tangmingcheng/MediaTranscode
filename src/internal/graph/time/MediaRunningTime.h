#pragma once

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
        const UInt128 scaledTicks = multiply(magnitude, scale);
        const UInt128 firstOverflowValue = multiply(
            maximumMagnitude + 1,
            static_cast<std::uint64_t>(timeBaseDenominator));
        if (scaledTicks >= firstOverflowValue) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaRunningTime tick rescale is not representable"));
        }

        const std::uint64_t rescaledMagnitude = divide(
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
    struct UInt128 {
        std::uint64_t high = 0;
        std::uint64_t low = 0;

        friend constexpr bool operator>=(UInt128 lhs, UInt128 rhs) noexcept
        {
            return lhs.high > rhs.high ||
                   (lhs.high == rhs.high && lhs.low >= rhs.low);
        }
    };

    static constexpr UInt128 multiply(std::uint64_t lhs, std::uint64_t rhs) noexcept
    {
        const std::uint64_t lhsLow = static_cast<std::uint32_t>(lhs);
        const std::uint64_t lhsHigh = lhs >> 32;
        const std::uint64_t rhsLow = static_cast<std::uint32_t>(rhs);
        const std::uint64_t rhsHigh = rhs >> 32;

        const std::uint64_t lowProduct = lhsLow * rhsLow;
        const std::uint64_t firstCross = lhsHigh * rhsLow + (lowProduct >> 32);
        const std::uint64_t firstCrossLow = static_cast<std::uint32_t>(firstCross);
        const std::uint64_t firstCrossHigh = firstCross >> 32;
        const std::uint64_t secondCross = lhsLow * rhsHigh + firstCrossLow;

        return UInt128{
            lhsHigh * rhsHigh + firstCrossHigh + (secondCross >> 32),
            (secondCross << 32) + static_cast<std::uint32_t>(lowProduct)};
    }

    static constexpr std::uint64_t divide(UInt128 dividend,
                                          std::uint32_t divisor) noexcept
    {
        const std::uint32_t limbs[4] = {
            static_cast<std::uint32_t>(dividend.high >> 32),
            static_cast<std::uint32_t>(dividend.high),
            static_cast<std::uint32_t>(dividend.low >> 32),
            static_cast<std::uint32_t>(dividend.low)};
        std::uint64_t remainder = 0;
        std::uint64_t quotient = 0;
        for (int index = 0; index < 4; ++index) {
            const std::uint64_t partial = (remainder << 32) | limbs[index];
            const std::uint32_t quotientLimb =
                static_cast<std::uint32_t>(partial / divisor);
            remainder = partial % divisor;
            if (index >= 2) {
                quotient = (quotient << 32) | quotientLimb;
            }
        }
        return quotient;
    }

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
