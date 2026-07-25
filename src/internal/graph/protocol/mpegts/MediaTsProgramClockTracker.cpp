#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"

#include <algorithm>
#include <limits>

namespace media::ffmpeg::graph {

namespace {

bool validPid(std::uint16_t pid) noexcept
{
    return pid <= 0x1fff;
}

::media::Status intervalStatus(std::int64_t delta,
                               const MediaTsProgramClockPolicy& policy)
{
    if (delta <= 0 || delta > policy.maximumGap27Mhz) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR gap or regression violates policy"));
    }
    const std::int64_t remainder = delta % policy.pcrInterval27Mhz;
    const std::int64_t residual = std::min(
        remainder, policy.pcrInterval27Mhz - remainder);
    if (residual > policy.maximumJitter27Mhz) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR jitter violates planned interval"));
    }
    return ::media::Status::success();
}

} // namespace

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
    if (policy.programNumber == 0 || !validPid(policy.pmtPid) ||
        !validPid(policy.pcrPid) || !validPid(policy.videoPid) ||
        !validPid(policy.audioPid) || policy.pcrInterval27Mhz <= 0 ||
        policy.maximumJitter27Mhz < 0 ||
        policy.maximumJitter27Mhz > policy.pcrInterval27Mhz / 2 ||
        policy.maximumGap27Mhz < policy.pcrInterval27Mhz) {
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
    MediaTsProgramClockTracker candidate = *this;
    auto status = candidate.observeCandidate(observation);
    if (!status) return status;
    *this = std::move(candidate);
    return ::media::Status::success();
}

::media::Status MediaTsProgramClockTracker::observeCandidate(
    const MediaTsPcrObservation& observation)
{
    auto identity = observeProgramIdentity(
        observation.programNumber, observation.pmtPid, observation.videoPid,
        observation.audioPid, observation.pcrPid);
    if (!identity) return identity;
    if (m_previousByteOffset && observation.byteOffset <= *m_previousByteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR byte offsets must increase strictly"));
    }
    if (observation.pcr27Mhz >= m_unwrapper.modulus()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR is outside the 27 MHz counter range"));
    }
    if (observation.discontinuity) {
        auto status = reacquire();
        if (!status) return status;
    }

    auto raw = MediaProtocolTimestamp::create(
        static_cast<std::int64_t>(observation.pcr27Mhz), 1, 27'000'000);
    if (!raw) return ::media::Status::failure(raw.error());
    auto unwrapped = m_unwrapper.unwrap(raw.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR regression or unwrap failure"));
    }
    const std::int64_t extended = unwrapped.timestamp->ticks();
    if (m_previousPcr) {
        auto interval = intervalStatus(extended - *m_previousPcr, m_policy);
        if (!interval) return interval;
        m_ready = true;
    }

    if (!m_generationPcrAnchor) {
        auto natural = MediaRunningTime::checkedFromTicks(extended, 1, 27'000'000);
        if (!natural) return ::media::Status::failure(natural.error());
        m_generationPcrAnchor = extended;
        m_generationSourceAnchor = m_lastPublishedSourceTime
            ? std::max(*m_lastPublishedSourceTime, natural.value())
            : natural.value();
    }
    auto elapsed = MediaRunningTime::checkedFromTicks(
        extended - *m_generationPcrAnchor, 1, 27'000'000);
    if (!elapsed) return ::media::Status::failure(elapsed.error());
    auto sourceTime = m_generationSourceAnchor->checkedAdd(elapsed.value());
    if (!sourceTime) return ::media::Status::failure(sourceTime.error());

    m_previousPcr = extended;
    m_previousByteOffset = observation.byteOffset;
    m_calibration = MediaTsPcrCalibration{m_generation, extended, sourceTime.value()};
    if (m_ready) m_lastPublishedSourceTime = sourceTime.value();
    return ::media::Status::success();
}

::media::Status MediaTsProgramClockTracker::observePcrContinuityLoss(std::uint16_t pid)
{
    if (pid != m_policy.pcrPid) return ::media::Status::success();
    MediaTsProgramClockTracker candidate = *this;
    auto status = candidate.reacquire();
    if (!status) return status;
    *this = std::move(candidate);
    return ::media::Status::success();
}

::media::Status MediaTsProgramClockTracker::observeElementaryContinuityLoss(
    std::uint16_t pid)
{
    if (pid != m_policy.videoPid && pid != m_policy.audioPid) {
        return ::media::Status::success();
    }
    MediaTsProgramClockTracker candidate = *this;
    auto status = candidate.reacquire();
    if (!status) return status;
    *this = std::move(candidate);
    return ::media::Status::success();
}

::media::Status MediaTsProgramClockTracker::observeProgramIdentity(
    std::uint16_t programNumber,
    std::uint16_t pmtPid,
    std::uint16_t videoPid,
    std::uint16_t audioPid,
    std::uint16_t pcrPid) const
{
    if (programNumber != m_policy.programNumber || pmtPid != m_policy.pmtPid ||
        videoPid != m_policy.videoPid || audioPid != m_policy.audioPid ||
        pcrPid != m_policy.pcrPid) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS selected identity changed from immutable plan"));
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

::media::Status MediaTsProgramClockTracker::reacquire()
{
    if (m_generation == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR generation exhausted"));
    }
    ++m_generation;
    m_unwrapper.reset(m_generation);
    m_previousPcr.reset();
    m_generationPcrAnchor.reset();
    m_generationSourceAnchor.reset();
    m_calibration.reset();
    m_ready = false;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
