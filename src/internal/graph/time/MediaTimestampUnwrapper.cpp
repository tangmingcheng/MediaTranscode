#include "internal/graph/time/MediaTimestampUnwrapper.h"

#include <limits>

namespace media::ffmpeg::graph {

namespace {

bool isSupportedBitWidth(std::uint8_t bitWidth) noexcept
{
    return bitWidth == 32 || bitWidth == 33;
}

constexpr std::uint64_t pcrModulus = (std::uint64_t{1} << 33) * 300;

} // namespace

::media::Result<MediaTimestampUnwrapper> MediaTimestampUnwrapper::create(
    std::uint8_t bitWidth,
    std::uint64_t generation)
{
    if (!isSupportedBitWidth(bitWidth)) {
        return ::media::Result<MediaTimestampUnwrapper>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaTimestampUnwrapper supports only 32 and 33-bit binary timestamps"));
    }

    return ::media::Result<MediaTimestampUnwrapper>::success(
        MediaTimestampUnwrapper(bitWidth,
                                std::uint64_t{1} << bitWidth,
                                generation));
}

::media::Result<MediaTimestampUnwrapper> MediaTimestampUnwrapper::createPcr(
    std::uint64_t generation)
{
    return ::media::Result<MediaTimestampUnwrapper>::success(
        MediaTimestampUnwrapper(42, pcrModulus, generation));
}

MediaTimestampUnwrapper::MediaTimestampUnwrapper(
    std::uint8_t bitWidth,
    std::uint64_t modulus,
    std::uint64_t generation) noexcept
    : m_bitWidth(bitWidth)
    , m_modulus(modulus)
    , m_generation(generation)
{
}

MediaTimestampUnwrapResult MediaTimestampUnwrapper::unwrap(
    std::uint64_t rawTimestamp) noexcept
{
    if (rawTimestamp >= m_modulus) {
        return discontinuity(MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    }

    if (!m_initialized) {
        m_lastRawTimestamp = rawTimestamp;
        m_lastUnwrappedTimestamp = static_cast<std::int64_t>(rawTimestamp);
        m_initialized = true;
        return {MediaTimestampUnwrapStatus::Value,
                MediaTimestampDiscontinuityReason::None,
                m_lastUnwrappedTimestamp,
                m_generation};
    }

    std::uint64_t forwardMovement = 0;
    const auto halfRange = m_modulus / 2;
    if (rawTimestamp >= m_lastRawTimestamp) {
        forwardMovement = rawTimestamp - m_lastRawTimestamp;
        if (forwardMovement >= halfRange && forwardMovement != 0) {
            return discontinuity(MediaTimestampDiscontinuityReason::AmbiguousMovement);
        }
    } else {
        const auto backwardMovement = m_lastRawTimestamp - rawTimestamp;
        if (backwardMovement <= halfRange) {
            return discontinuity(MediaTimestampDiscontinuityReason::BackwardMovement);
        }
        forwardMovement = m_modulus - backwardMovement;
    }

    const auto maximumIncrement = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max() - m_lastUnwrappedTimestamp);
    if (forwardMovement > maximumIncrement) {
        return discontinuity(MediaTimestampDiscontinuityReason::UnwrappedValueOverflow);
    }

    m_lastRawTimestamp = rawTimestamp;
    m_lastUnwrappedTimestamp += static_cast<std::int64_t>(forwardMovement);
    return {MediaTimestampUnwrapStatus::Value,
            MediaTimestampDiscontinuityReason::None,
            m_lastUnwrappedTimestamp,
            m_generation};
}

void MediaTimestampUnwrapper::reset(std::uint64_t generation) noexcept
{
    m_generation = generation;
    m_lastRawTimestamp = 0;
    m_lastUnwrappedTimestamp = 0;
    m_initialized = false;
}

std::uint8_t MediaTimestampUnwrapper::bitWidth() const noexcept
{
    return m_bitWidth;
}

std::uint64_t MediaTimestampUnwrapper::generation() const noexcept
{
    return m_generation;
}

MediaTimestampUnwrapResult MediaTimestampUnwrapper::discontinuity(
    MediaTimestampDiscontinuityReason reason) const noexcept
{
    return {MediaTimestampUnwrapStatus::Discontinuity,
            reason,
            m_lastUnwrappedTimestamp,
            m_generation};
}

} // namespace media::ffmpeg::graph
