#include "internal/graph/protocol/rtp/MediaRtpSourceClockMapper.h"

#include <cmath>
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::int64_t NanosecondsPerSecond = 1'000'000'000;
constexpr std::int64_t RtpModulus = std::int64_t{1} << 32;
constexpr std::int64_t RtpHalfRange = RtpModulus / 2;

::media::ErrorInfo invalid(const char* message)
{
    return ::media::ErrorInfo::invalidArgument(message);
}

bool ntpLess(const MediaRtcpNtpTimestamp& lhs, const MediaRtcpNtpTimestamp& rhs) noexcept
{
    return lhs.seconds < rhs.seconds ||
           (lhs.seconds == rhs.seconds && lhs.fraction < rhs.fraction);
}

::media::Result<MediaRunningTime> ntpSourceTime(const MediaRtcpNtpTimestamp& ntp)
{
    const std::uint64_t fractionNs =
        (static_cast<std::uint64_t>(ntp.fraction) * NanosecondsPerSecond) >> 32;
    const std::uint64_t secondsNs =
        static_cast<std::uint64_t>(ntp.seconds) * NanosecondsPerSecond;
    if (secondsNs > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - fractionNs) {
        return ::media::Result<MediaRunningTime>::failure(invalid("RTCP NTP source time is not representable"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(secondsNs + fractionNs)));
}

} // namespace

::media::Result<MediaRtpSourceClockMapper> MediaRtpSourceClockMapper::create(
    MediaRtpSourceClockMapperConfig config,
    std::uint64_t generation)
{
    if (config.clockRate <= 0 || config.senderReportTimeoutNs <= 0 ||
        config.maximumExtrapolationNs <= config.senderReportTimeoutNs ||
        config.maximumResidualNs <= 0 || config.maximumRateErrorPpm <= 0 ||
        config.maximumRateErrorPpm >= 1'000'000) {
        return ::media::Result<MediaRtpSourceClockMapper>::failure(
            invalid("RTP source clock mapper requires complete ordered planner thresholds"));
    }
    auto srUnwrapper = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, generation);
    auto packetUnwrapper = MediaTimestampUnwrapper::create(MediaTimestampCounterKind::Rtp32, generation);
    if (!srUnwrapper) return ::media::Result<MediaRtpSourceClockMapper>::failure(srUnwrapper.error());
    if (!packetUnwrapper) return ::media::Result<MediaRtpSourceClockMapper>::failure(packetUnwrapper.error());
    return ::media::Result<MediaRtpSourceClockMapper>::success(
        MediaRtpSourceClockMapper(config,
                                  generation,
                                  std::move(srUnwrapper).value(),
                                  std::move(packetUnwrapper).value()));
}

MediaRtpSourceClockMapper::MediaRtpSourceClockMapper(
    MediaRtpSourceClockMapperConfig config,
    std::uint64_t generation,
    MediaTimestampUnwrapper senderReportUnwrapper,
    MediaTimestampUnwrapper packetUnwrapper) noexcept
    : m_config(config)
    , m_generation(generation)
    , m_senderReportUnwrapper(std::move(senderReportUnwrapper))
    , m_packetUnwrapper(std::move(packetUnwrapper))
{
}

::media::Status MediaRtpSourceClockMapper::observeSenderReport(
    const MediaRtcpClockEvidence& evidence)
{
    if (evidence.senderReportObservedAtNs < 0 || evidence.cnameObservedAtNs < 0) {
        invalidate();
        return ::media::Status::failure(
            invalid("RTP source clock evidence observation times must be non-negative"));
    }
    if (evidence.generation != m_generation ||
        (m_config.requireCname && evidence.cname.empty()) ||
        evidence.observedMediaSsrc != evidence.senderReportSsrc ||
        evidence.observedMediaSsrc != evidence.cnameSsrc) {
        invalidate();
        return ::media::Status::failure(invalid("RTP sender report generation or CNAME is invalid"));
    }
    if (m_calibration &&
        (evidence.observedMediaSsrc != m_calibration->ssrc ||
         (m_config.requireCname && evidence.cname != m_calibration->cname))) {
        invalidate();
        return ::media::Status::failure(invalid("RTP sender identity changed"));
    }
    if (m_lastNtp && ntpLess(evidence.ntp, *m_lastNtp)) {
        invalidate();
        return ::media::Status::failure(invalid("RTP sender report NTP regressed"));
    }
    if (m_lastNtp && evidence.ntp == *m_lastNtp) {
        if (!m_calibration || evidence.rtpTimestamp != m_calibration->rtpAnchor) {
            invalidate();
            return ::media::Status::failure(
                invalid("RTP sender report changed RTP timestamp at the same NTP instant"));
        }
        m_calibration->senderReportObservedAtNs = evidence.senderReportObservedAtNs;
        return ::media::Status::success();
    }

    auto raw = MediaProtocolTimestamp::create(evidence.rtpTimestamp, 1, m_config.clockRate);
    if (!raw) return ::media::Status::failure(raw.error());
    const auto unwrapped = m_senderReportUnwrapper.unwrap(raw.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        invalidate();
        return ::media::Status::failure(invalid("RTP sender report timestamp movement is invalid"));
    }
    auto actualSource = ntpSourceTime(evidence.ntp);
    if (!actualSource) {
        invalidate();
        return ::media::Status::failure(actualSource.error());
    }

    const std::int64_t extended = unwrapped.timestamp->ticks();
    std::int64_t rateNumerator = NanosecondsPerSecond;
    std::int64_t rateDenominator = m_config.clockRate;
    MediaRunningTime continuousAnchor = actualSource.value();
    if (m_calibration) {
        const std::int64_t deltaTicks = extended - m_calibration->extendedRtpAnchor;
        const std::int64_t deltaNtp = actualSource.value().nanoseconds() -
                                      m_calibration->actualSenderReportSourceTime.nanoseconds();
        if (deltaTicks <= 0 || deltaNtp <= 0) {
            invalidate();
            return ::media::Status::failure(invalid("RTP sender report slope is not positive"));
        }
        const long double nominalDelta =
            static_cast<long double>(deltaTicks) * NanosecondsPerSecond / m_config.clockRate;
        const long double residual = std::fabs(static_cast<long double>(deltaNtp) - nominalDelta);
        if (residual > m_config.maximumResidualNs) {
            invalidate();
            return ::media::Status::failure(invalid("RTP sender report residual exceeds planner threshold"));
        }
        const long double observedNanosecondsPerSecond =
            static_cast<long double>(deltaNtp) * m_config.clockRate / deltaTicks;
        const long double rateErrorPpm =
            std::fabs(observedNanosecondsPerSecond - NanosecondsPerSecond) * 1'000'000 /
            NanosecondsPerSecond;
        if (rateErrorPpm > m_config.maximumRateErrorPpm) {
            invalidate();
            return ::media::Status::failure(
                invalid("RTP sender report rate exceeds planner-derived bound"));
        }
        if (!m_estimatorOriginRtp || !m_estimatorOriginSourceTime) {
            invalidate();
            return ::media::Status::failure(
                ::media::ErrorInfo::internalError("RTP source clock estimator origin is unavailable"));
        }
        auto continuous = mapExtended(extended);
        if (!continuous) {
            invalidate();
            return ::media::Status::failure(continuous.error());
        }
        continuousAnchor = continuous.value();
        rateNumerator = actualSource.value().nanoseconds() -
                        m_estimatorOriginSourceTime->nanoseconds();
        rateDenominator = extended - *m_estimatorOriginRtp;
    }

    m_calibration = MediaRtpSourceClockCalibration{
        evidence.observedMediaSsrc,
        evidence.cname,
        evidence.generation,
        evidence.rtpTimestamp,
        extended,
        actualSource.value(),
        continuousAnchor,
        rateNumerator,
        rateDenominator,
        evidence.senderReportObservedAtNs,
        MediaRtpSourceClockConfidence::Locked};
    m_lastNtp = evidence.ntp;
    if (!m_estimatorOriginRtp) {
        m_estimatorOriginRtp = extended;
        m_estimatorOriginSourceTime = actualSource.value();
    }
    return ::media::Status::success();
}

::media::Result<MediaRtpMappedSourceTime> MediaRtpSourceClockMapper::map(
    std::uint32_t rtpTimestamp,
    std::int64_t observedAtNs)
{
    auto state = confidence(observedAtNs);
    if (!state) return ::media::Result<MediaRtpMappedSourceTime>::failure(state.error());
    auto raw = MediaProtocolTimestamp::create(rtpTimestamp, 1, m_config.clockRate);
    if (!raw) return ::media::Result<MediaRtpMappedSourceTime>::failure(raw.error());
    const auto unwrapped = m_packetUnwrapper.unwrap(raw.value());
    if (unwrapped.status != MediaTimestampUnwrapStatus::Value || !unwrapped.timestamp) {
        return ::media::Result<MediaRtpMappedSourceTime>::failure(
            invalid("RTP media timestamp cannot be unwrapped"));
    }
    if (!m_packetWrapAdjustment) {
        std::int64_t aligned = unwrapped.timestamp->ticks();
        while (aligned - m_calibration->extendedRtpAnchor > RtpHalfRange) aligned -= RtpModulus;
        while (m_calibration->extendedRtpAnchor - aligned > RtpHalfRange) aligned += RtpModulus;
        m_packetWrapAdjustment = aligned - unwrapped.timestamp->ticks();
    }
    const std::int64_t extended = unwrapped.timestamp->ticks() + *m_packetWrapAdjustment;
    auto mapped = mapExtended(extended);
    if (!mapped) return ::media::Result<MediaRtpMappedSourceTime>::failure(mapped.error());
    return ::media::Result<MediaRtpMappedSourceTime>::success(
        MediaRtpMappedSourceTime{mapped.value(), m_generation, state.value()});
}

::media::Result<MediaRtpSourceClockCalibration> MediaRtpSourceClockMapper::calibration(
    std::int64_t observedAtNs) const
{
    auto state = confidence(observedAtNs);
    if (!state) return ::media::Result<MediaRtpSourceClockCalibration>::failure(state.error());
    MediaRtpSourceClockCalibration result = *m_calibration;
    result.confidence = state.value();
    return ::media::Result<MediaRtpSourceClockCalibration>::success(std::move(result));
}

void MediaRtpSourceClockMapper::reset(std::uint64_t generation) noexcept
{
    m_generation = generation;
    m_senderReportUnwrapper.reset(generation);
    m_packetUnwrapper.reset(generation);
    m_calibration.reset();
    m_lastNtp.reset();
    m_packetWrapAdjustment.reset();
    m_estimatorOriginRtp.reset();
    m_estimatorOriginSourceTime.reset();
}

::media::Result<MediaRtpSourceClockConfidence> MediaRtpSourceClockMapper::confidence(
    std::int64_t observedAtNs) const
{
    if (!m_calibration) {
        return ::media::Result<MediaRtpSourceClockConfidence>::failure(
            ::media::ErrorInfo::notInitialized("RTP source clock has no sender report calibration"));
    }
    if (observedAtNs < m_calibration->senderReportObservedAtNs) {
        return ::media::Result<MediaRtpSourceClockConfidence>::failure(
            invalid("RTP source clock observation time regressed"));
    }
    const std::int64_t age = observedAtNs - m_calibration->senderReportObservedAtNs;
    if (age <= m_config.senderReportTimeoutNs) {
        return ::media::Result<MediaRtpSourceClockConfidence>::success(
            MediaRtpSourceClockConfidence::Locked);
    }
    if (age <= m_config.maximumExtrapolationNs) {
        return ::media::Result<MediaRtpSourceClockConfidence>::success(
            MediaRtpSourceClockConfidence::Degraded);
    }
    return ::media::Result<MediaRtpSourceClockConfidence>::failure(
        ::media::ErrorInfo::notInitialized("RTP source clock extrapolation expired"));
}

::media::Result<MediaRunningTime> MediaRtpSourceClockMapper::mapExtended(
    std::int64_t extendedTimestamp) const
{
    if (!m_calibration || m_calibration->rateDenominatorTicks <= 0) {
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::notInitialized("RTP source clock calibration is unavailable"));
    }
    const long double deltaTicks = static_cast<long double>(
        extendedTimestamp - m_calibration->extendedRtpAnchor);
    const long double deltaNs = deltaTicks * m_calibration->rateNumeratorNs /
                                m_calibration->rateDenominatorTicks;
    const long double mapped = m_calibration->continuousSourceAnchor.nanoseconds() + deltaNs;
    if (mapped < std::numeric_limits<std::int64_t>::min() ||
        mapped > std::numeric_limits<std::int64_t>::max()) {
        return ::media::Result<MediaRunningTime>::failure(
            invalid("RTP mapped source time is not representable"));
    }
    return ::media::Result<MediaRunningTime>::success(
        MediaRunningTime::fromNanoseconds(static_cast<std::int64_t>(std::llround(mapped))));
}

void MediaRtpSourceClockMapper::invalidate() noexcept
{
    m_senderReportUnwrapper.reset(m_generation);
    m_packetUnwrapper.reset(m_generation);
    m_calibration.reset();
    m_lastNtp.reset();
    m_packetWrapAdjustment.reset();
    m_estimatorOriginRtp.reset();
    m_estimatorOriginSourceTime.reset();
}

} // namespace media::ffmpeg::graph
