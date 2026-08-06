#include "internal/graph/protocol/mpegts/MediaTsProgramClockTracker.h"

#include <algorithm>
#include <limits>
#include <type_traits>

namespace media::ffmpeg::graph {

namespace {

bool validPid(std::uint16_t pid) noexcept
{
    return pid > 0 && pid < 0x1fff;
}

bool requiresReacquisition(std::int64_t delta,
                           const MediaTsProgramClockPolicy& policy) noexcept
{
    const auto maximumGap = std::visit(
        [](const auto& selected) { return selected.maximumGap27Mhz; }, policy);
    return delta <= 0 || delta > maximumGap;
}

std::uint16_t pcrPid(const MediaTsProgramClockPolicy& policy) noexcept
{
    return std::visit(
        [](const auto& selected) { return selected.pcrPid; }, policy);
}

bool validPolicy(const MediaTsProgramClockPolicy& policy) noexcept
{
    return std::visit(
        [](const auto& selected) {
            using Policy = std::decay_t<decltype(selected)>;
            const bool common = selected.programNumber > 0 &&
                validPid(selected.pmtPid) && validPid(selected.pcrPid) &&
                validPid(selected.videoPid) &&
                selected.maximumGap27Mhz > 0;
            if constexpr (std::is_same_v<
                              Policy,
                              MediaTsAudioVideoProgramClockPolicy>) {
                return common && validPid(selected.audioPid) &&
                    selected.audioPid != selected.videoPid;
            }
            return common;
        },
        policy);
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
    if (!validPolicy(policy)) {
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
    if (observation.pcrPid != pcrPid(m_policy)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS PCR observation conflicts with immutable plan"));
    }
    if (m_previousByteOffset && observation.byteOffset <= *m_previousByteOffset) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR byte offsets must increase strictly"));
    }
    if (observation.pcr27Mhz >= m_unwrapper.modulus()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR is outside the 27 MHz counter range"));
    }
    if (observation.discontinuity) {
        if (auto status = reacquire(); !status) return status;
    }
    auto raw = MediaProtocolTimestamp::create(
        static_cast<std::int64_t>(observation.pcr27Mhz), 1, 27'000'000);
    if (!raw) return ::media::Status::failure(raw.error());
    auto unwrapped = m_unwrapper.unwrap(raw.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        if (auto status = reacquire(); !status) return status;
        unwrapped = m_unwrapper.unwrap(raw.value());
        if (unwrapped.status != MediaTimestampUnwrapStatus::Value ||
            !unwrapped.timestamp) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS PCR failed to establish a new clock generation"));
        }
    }
    std::int64_t extended = unwrapped.timestamp->ticks();
    if (m_previousPcr &&
        requiresReacquisition(extended - *m_previousPcr, m_policy)) {
        if (auto status = reacquire(); !status) return status;
        unwrapped = m_unwrapper.unwrap(raw.value());
        if (unwrapped.status != MediaTimestampUnwrapStatus::Value ||
            !unwrapped.timestamp) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MPEG-TS PCR failed to establish a new clock generation"));
        }
        extended = unwrapped.timestamp->ticks();
    }
    if (m_previousPcr) {
        m_ready = true;
        m_reacquiring = false;
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
    if (pid != pcrPid(m_policy)) return ::media::Status::success();
    MediaTsProgramClockTracker candidate = *this;
    auto status = candidate.reacquire();
    if (!status) return status;
    *this = std::move(candidate);
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
    if (!m_reacquiring &&
        m_generation == std::numeric_limits<std::uint64_t>::max()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MPEG-TS PCR generation exhausted"));
    }
    if (!m_reacquiring) ++m_generation;
    m_unwrapper.reset(m_generation);
    m_previousPcr.reset();
    m_generationPcrAnchor.reset();
    m_generationSourceAnchor.reset();
    m_calibration.reset();
    m_ready = false;
    m_reacquiring = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
