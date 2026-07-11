#pragma once

#include "media_transcode/Result.h"

#include <compare>
#include <cstdint>
#include <limits>
#include <string>

extern "C" {
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
}

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

        constexpr AVRational nanosecondTimeBase{1, 1'000'000'000};
        const auto nanoseconds = av_rescale_q_rnd(
            ticks,
            AVRational{timeBaseNumerator, timeBaseDenominator},
            nanosecondTimeBase,
            AV_ROUND_ZERO);
        if (nanoseconds == std::numeric_limits<std::int64_t>::min()) {
            return ::media::Result<MediaRunningTime>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaRunningTime tick rescale is not representable"));
        }

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
