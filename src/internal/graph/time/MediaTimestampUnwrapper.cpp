#include "internal/graph/time/MediaTimestampUnwrapper.h"

#include <limits>

namespace media::ffmpeg::graph {

namespace {

constexpr std::uint64_t rtpModulus = std::uint64_t{1} << 32;
constexpr std::uint64_t ptsDtsModulus = std::uint64_t{1} << 33;
constexpr std::uint64_t pcrModulus = ptsDtsModulus * 300;

std::uint64_t modulusFor(MediaTimestampCounterKind counterKind) noexcept
{
    switch (counterKind) {
    case MediaTimestampCounterKind::Rtp32:
        return rtpModulus;
    case MediaTimestampCounterKind::MpegTsPtsDts33:
        return ptsDtsModulus;
    case MediaTimestampCounterKind::MpegTsPcr27Mhz:
        return pcrModulus;
    }
    return 0;
}

} // namespace

::media::Result<MediaTimestampUnwrapper> MediaTimestampUnwrapper::create(
    MediaTimestampCounterKind counterKind,
    std::uint64_t generation)
{
    const std::uint64_t modulus = modulusFor(counterKind);
    if (modulus == 0) {
        return ::media::Result<MediaTimestampUnwrapper>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaTimestampUnwrapper requires a supported protocol counter"));
    }
    return ::media::Result<MediaTimestampUnwrapper>::success(
        MediaTimestampUnwrapper(counterKind, modulus, generation));
}

MediaTimestampUnwrapper::MediaTimestampUnwrapper(
    MediaTimestampCounterKind counterKind,
    std::uint64_t modulus,
    std::uint64_t generation) noexcept
    : m_counterKind(counterKind)
    , m_modulus(modulus)
    , m_generation(generation)
{
}

MediaTimestampUnwrapResult MediaTimestampUnwrapper::unwrap(
    MediaProtocolTimestamp rawTimestamp) noexcept
{
    if (rawTimestamp.ticks() < 0 ||
        static_cast<std::uint64_t>(rawTimestamp.ticks()) >= m_modulus) {
        return discontinuity(MediaTimestampDiscontinuityReason::RawValueOutOfRange);
    }
    if (!rawTimestamp.hasValidTimeBase() ||
        (m_initialized &&
         (rawTimestamp.timeBaseNumerator() != m_timeBaseNumerator ||
          rawTimestamp.timeBaseDenominator() != m_timeBaseDenominator))) {
        return discontinuity(MediaTimestampDiscontinuityReason::TimeBaseMismatch);
    }

    const std::uint64_t rawTicks = static_cast<std::uint64_t>(rawTimestamp.ticks());
    if (!m_initialized) {
        m_lastRawTimestamp = rawTicks;
        m_lastUnwrappedTimestamp = rawTimestamp.ticks();
        m_timeBaseNumerator = rawTimestamp.timeBaseNumerator();
        m_timeBaseDenominator = rawTimestamp.timeBaseDenominator();
        m_initialized = true;
        return valueResult();
    }

    const std::uint64_t halfRange = m_modulus / 2;
    std::uint64_t forwardMovement = 0;
    if (rawTicks >= m_lastRawTimestamp) {
        const std::uint64_t directMovement = rawTicks - m_lastRawTimestamp;
        if (directMovement == halfRange) {
            return discontinuity(MediaTimestampDiscontinuityReason::AmbiguousMovement);
        }
        if (directMovement > halfRange) {
            return discontinuity(MediaTimestampDiscontinuityReason::BackwardMovement);
        }
        forwardMovement = directMovement;
    } else {
        const std::uint64_t backwardMovement = m_lastRawTimestamp - rawTicks;
        if (backwardMovement == halfRange) {
            return discontinuity(MediaTimestampDiscontinuityReason::AmbiguousMovement);
        }
        if (backwardMovement < halfRange) {
            return discontinuity(MediaTimestampDiscontinuityReason::BackwardMovement);
        }
        forwardMovement = m_modulus - backwardMovement;
    }

    const std::uint64_t maximumIncrement = static_cast<std::uint64_t>(
        std::numeric_limits<std::int64_t>::max() - m_lastUnwrappedTimestamp);
    if (forwardMovement > maximumIncrement) {
        return discontinuity(MediaTimestampDiscontinuityReason::UnwrappedValueOverflow);
    }

    m_lastRawTimestamp = rawTicks;
    m_lastUnwrappedTimestamp += static_cast<std::int64_t>(forwardMovement);
    return valueResult();
}

void MediaTimestampUnwrapper::reset(std::uint64_t generation) noexcept
{
    m_generation = generation;
    m_lastRawTimestamp = 0;
    m_lastUnwrappedTimestamp = 0;
    m_timeBaseNumerator = 0;
    m_timeBaseDenominator = 0;
    m_initialized = false;
}

MediaTimestampCounterKind MediaTimestampUnwrapper::counterKind() const noexcept
{
    return m_counterKind;
}

std::uint64_t MediaTimestampUnwrapper::modulus() const noexcept
{
    return m_modulus;
}

std::uint64_t MediaTimestampUnwrapper::generation() const noexcept
{
    return m_generation;
}

MediaTimestampUnwrapResult MediaTimestampUnwrapper::valueResult() const noexcept
{
    return {MediaTimestampUnwrapStatus::Value,
            MediaTimestampDiscontinuityReason::None,
            MediaProtocolTimestamp(m_lastUnwrappedTimestamp,
                                   m_timeBaseNumerator,
                                   m_timeBaseDenominator),
            m_generation};
}

MediaTimestampUnwrapResult MediaTimestampUnwrapper::discontinuity(
    MediaTimestampDiscontinuityReason reason) const noexcept
{
    return {MediaTimestampUnwrapStatus::Discontinuity,
            reason,
            MediaProtocolTimestamp(m_lastUnwrappedTimestamp,
                                   m_timeBaseNumerator,
                                   m_timeBaseDenominator),
            m_generation};
}

} // namespace media::ffmpeg::graph
