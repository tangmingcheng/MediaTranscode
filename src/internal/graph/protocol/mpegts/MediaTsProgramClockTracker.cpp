#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"

#include <cstdlib>
#include <limits>

namespace media::ffmpeg::graph {

MediaTsProgramClockTracker::MediaTsProgramClockTracker(
    MediaTsProgramClockPolicy policy,
    std::uint64_t generation,
    MediaTimestampUnwrapper unwrapper) noexcept
    : m_policy(policy)
    , m_generation(generation)
    , m_unwrapper(std::move(unwrapper))
{
}

::media::Result<MediaTsProgramClockTracker> MediaTsProgramClockTracker::create(
    MediaTsProgramClockPolicy policy,
    std::uint64_t generation)
{
    if (policy.programNumber == 0 || policy.pmtPid > 0x1fff ||
        policy.pcrPid > 0x1fff || policy.pcrInterval27Mhz <= 0 ||
        policy.maximumJitter27Mhz < 0 || policy.maximumGap27Mhz < policy.pcrInterval27Mhz) {
        return ::media::Result<MediaTsProgramClockTracker>::failure(
            ::media::ErrorInfo::invalidArgument("Invalid immutable MPEG-TS PCR policy"));
    }
    auto unwrapper = MediaTimestampUnwrapper::create(
        MediaTimestampCounterKind::MpegTsPcr27Mhz, generation);
    if (!unwrapper) {
        return ::media::Result<MediaTsProgramClockTracker>::failure(unwrapper.error());
    }
    return ::media::Result<MediaTsProgramClockTracker>::success(
        MediaTsProgramClockTracker(policy, generation, std::move(unwrapper.value())));
}

::media::Status MediaTsProgramClockTracker::observe(
    const MediaTsPcrObservation& observation)
{
    auto identity = observeProgramIdentity(observation.programNumber,
                                           observation.pmtPid,
                                           observation.pcrPid);
    if (!identity) return identity;
    if (observation.discontinuity) reacquire();

    auto raw = MediaProtocolTimestamp::create(
        static_cast<std::int64_t>(observation.pcr27Mhz), 1, 27'000'000);
    if (!raw) return ::media::Status::failure(raw.error());
    auto candidateUnwrapper = m_unwrapper;
    auto unwrapped = candidateUnwrapper.unwrap(raw.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR regression or unwrap failure"));
    }
    const std::int64_t extended = unwrapped.timestamp->ticks();
    if (m_previousPcr) {
        const std::int64_t delta = extended - *m_previousPcr;
        if (delta <= 0 || delta > m_policy.maximumGap27Mhz) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS PCR gap or regression violates policy"));
        }
        const std::int64_t multiple =
            (delta + m_policy.pcrInterval27Mhz / 2) / m_policy.pcrInterval27Mhz;
        if (multiple <= 0 ||
            std::llabs(delta - multiple * m_policy.pcrInterval27Mhz) >
                m_policy.maximumJitter27Mhz) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("MPEG-TS PCR jitter violates planned interval"));
        }
        m_ready = true;
    }
    auto sourceTime = MediaRunningTime::checkedFromTicks(extended, 1, 27'000'000);
    if (!sourceTime) return ::media::Status::failure(sourceTime.error());
    m_previousPcr = extended;
    m_calibration = MediaTsPcrCalibration{m_generation, extended, sourceTime.value()};
    m_unwrapper = std::move(candidateUnwrapper);
    return ::media::Status::success();
}

::media::Status MediaTsProgramClockTracker::observeContinuityLoss(std::uint16_t pid)
{
    if (pid != m_policy.pcrPid) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Continuity loss PID does not match selected PCR PID"));
    }
    reacquire();
    return ::media::Status::success();
}

::media::Status MediaTsProgramClockTracker::observeProgramIdentity(
    std::uint16_t programNumber,
    std::uint16_t pmtPid,
    std::uint16_t pcrPid) const
{
    if (programNumber != m_policy.programNumber || pmtPid != m_policy.pmtPid ||
        pcrPid != m_policy.pcrPid) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS program or PCR PID changed from immutable plan"));
    }
    return ::media::Status::success();
}

::media::Result<MediaTsPcrCalibration> MediaTsProgramClockTracker::calibration() const
{
    if (!m_ready || !m_calibration) {
        return ::media::Result<MediaTsPcrCalibration>::failure(
            ::media::ErrorInfo::notInitialized("MPEG-TS PCR calibration is not locked"));
    }
    return ::media::Result<MediaTsPcrCalibration>::success(*m_calibration);
}

void MediaTsProgramClockTracker::reacquire()
{
    if (m_generation != std::numeric_limits<std::uint64_t>::max()) ++m_generation;
    m_unwrapper.reset(m_generation);
    m_previousPcr.reset();
    m_calibration.reset();
    m_ready = false;
}

} // namespace media::ffmpeg::graph
