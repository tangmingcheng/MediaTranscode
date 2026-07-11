#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaProtocolTimestamp final {
public:
    constexpr MediaProtocolTimestamp(std::int64_t ticks,
                                     int timeBaseNumerator,
                                     int timeBaseDenominator) noexcept
        : m_ticks(ticks)
        , m_timeBaseNumerator(timeBaseNumerator)
        , m_timeBaseDenominator(timeBaseDenominator)
    {
    }

    constexpr std::int64_t ticks() const noexcept { return m_ticks; }
    constexpr int timeBaseNumerator() const noexcept { return m_timeBaseNumerator; }
    constexpr int timeBaseDenominator() const noexcept { return m_timeBaseDenominator; }

    constexpr bool hasValidTimeBase() const noexcept
    {
        return m_timeBaseNumerator > 0 && m_timeBaseDenominator > 0;
    }

    friend constexpr bool operator==(MediaProtocolTimestamp,
                                     MediaProtocolTimestamp) noexcept = default;

private:
    std::int64_t m_ticks = 0;
    int m_timeBaseNumerator = 0;
    int m_timeBaseDenominator = 0;
};

enum class MediaTimestampCounterKind {
    Rtp32,
    MpegTsPtsDts33,
    MpegTsPcr27Mhz
};

enum class MediaTimestampUnwrapStatus {
    Value,
    Discontinuity
};

enum class MediaTimestampDiscontinuityReason {
    None,
    RawValueOutOfRange,
    TimeBaseMismatch,
    BackwardMovement,
    AmbiguousMovement,
    UnwrappedValueOverflow
};

struct MediaTimestampUnwrapResult {
    MediaTimestampUnwrapStatus status = MediaTimestampUnwrapStatus::Discontinuity;
    MediaTimestampDiscontinuityReason reason =
        MediaTimestampDiscontinuityReason::None;
    MediaProtocolTimestamp timestamp{0, 0, 0};
    std::uint64_t generation = 0;
};

class MediaTimestampUnwrapper final {
public:
    static ::media::Result<MediaTimestampUnwrapper> create(
        MediaTimestampCounterKind counterKind,
        std::uint64_t generation);

    MediaTimestampUnwrapResult unwrap(MediaProtocolTimestamp rawTimestamp) noexcept;
    void reset(std::uint64_t generation) noexcept;

    MediaTimestampCounterKind counterKind() const noexcept;
    std::uint64_t modulus() const noexcept;
    std::uint64_t generation() const noexcept;

private:
    MediaTimestampUnwrapper(MediaTimestampCounterKind counterKind,
                            std::uint64_t modulus,
                            std::uint64_t generation) noexcept;

    MediaTimestampUnwrapResult valueResult() const noexcept;
    MediaTimestampUnwrapResult discontinuity(
        MediaTimestampDiscontinuityReason reason) const noexcept;

    MediaTimestampCounterKind m_counterKind = MediaTimestampCounterKind::Rtp32;
    std::uint64_t m_modulus = 0;
    std::uint64_t m_generation = 0;
    std::uint64_t m_lastRawTimestamp = 0;
    std::int64_t m_lastUnwrappedTimestamp = 0;
    int m_timeBaseNumerator = 0;
    int m_timeBaseDenominator = 0;
    bool m_initialized = false;
};

} // namespace media::ffmpeg::graph
