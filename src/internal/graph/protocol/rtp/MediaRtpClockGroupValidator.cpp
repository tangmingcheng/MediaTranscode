#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"

#include <limits>
#include <optional>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status invalid(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

bool matchingCalibration(const MediaRtcpClockEvidence& evidence,
                         const MediaRtpSourceClockCalibration& calibration) noexcept
{
    return evidence.observedMediaSsrc == evidence.senderReportSsrc &&
           evidence.observedMediaSsrc == evidence.cnameSsrc &&
           evidence.observedMediaSsrc == calibration.ssrc &&
           evidence.cname == calibration.cname &&
           evidence.generation == calibration.generation &&
           evidence.rtpTimestamp == calibration.rtpAnchor &&
           evidence.senderReportObservedAtNs == calibration.senderReportObservedAtNs;
}

std::optional<std::int64_t> checkedSubtract(std::int64_t lhs,
                                            std::int64_t rhs) noexcept
{
    if ((rhs > 0 && lhs < std::numeric_limits<std::int64_t>::min() + rhs) ||
        (rhs < 0 && lhs > std::numeric_limits<std::int64_t>::max() + rhs)) {
        return std::nullopt;
    }
    return lhs - rhs;
}

std::optional<std::int64_t> initialAcquisitionClockOffsetSkew(
    const MediaRtcpClockEvidence& videoEvidence,
    const MediaRtpSourceClockCalibration& videoCalibration,
    const MediaRtcpClockEvidence& audioEvidence,
    const MediaRtpSourceClockCalibration& audioCalibration) noexcept
{
    const auto videoOffset = checkedSubtract(
        videoCalibration.actualSenderReportSourceTime.nanoseconds(),
        videoEvidence.senderReportObservedAtNs);
    const auto audioOffset = checkedSubtract(
        audioCalibration.actualSenderReportSourceTime.nanoseconds(),
        audioEvidence.senderReportObservedAtNs);
    if (!videoOffset || !audioOffset) return std::nullopt;
    return *videoOffset >= *audioOffset
        ? checkedSubtract(*videoOffset, *audioOffset)
        : checkedSubtract(*audioOffset, *videoOffset);
}

} // namespace

::media::Result<MediaRtpClockGroupValidator> MediaRtpClockGroupValidator::create(
    MediaRtpClockGroupValidatorConfig config)
{
    if (config.senderReportTimeoutNs <= 0 ||
        config.maximumExtrapolationNs <= config.senderReportTimeoutNs ||
        config.maximumInterStreamClockOffsetSkewNs <= 0 ||
        config.videoCnameTimeoutNs <= 0 ||
        config.audioCnameTimeoutNs <= 0 ||
        config.commonEpochPolicy !=
            MediaRtpCommonEpochPolicy::EarliestLockedSenderReportSourceTime) {
        return ::media::Result<MediaRtpClockGroupValidator>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP clock group validator requires complete ordered planner thresholds"));
    }
    return ::media::Result<MediaRtpClockGroupValidator>::success(
        MediaRtpClockGroupValidator(config));
}

MediaRtpClockGroupValidator::MediaRtpClockGroupValidator(
    MediaRtpClockGroupValidatorConfig config) noexcept
    : m_config(config)
{
}

::media::Status MediaRtpClockGroupValidator::observe(
    MediaStreamKind streamKind,
    const MediaRtcpClockEvidence& evidence,
    MediaRtpSourceClockCalibration calibration)
{
    if ((streamKind != MediaStreamKind::Video && streamKind != MediaStreamKind::Audio) ||
        (m_config.requireMatchingCname && evidence.cname.empty()) ||
        evidence.senderReportObservedAtNs < 0 ||
        evidence.cnameObservedAtNs < 0 || !matchingCalibration(evidence, calibration)) {
        clear(true);
        return invalid("RTP clock group evidence identity is invalid");
    }

    StreamState observed{evidence, std::move(calibration)};
    if (m_phase == Phase::ActiveGeneration && m_video && m_audio &&
        !m_reacquireRequired) {
        const StreamState& committed =
            streamKind == MediaStreamKind::Video ? *m_video : *m_audio;
        if (observed.evidence.observedMediaSsrc !=
                committed.evidence.observedMediaSsrc ||
            (m_config.requireMatchingCname &&
             observed.evidence.cname != committed.evidence.cname) ||
            observed.evidence.generation != committed.evidence.generation) {
            clear(true);
            return invalid(
                "RTP clock group active stream identity or generation changed");
        }

        std::optional<StreamState>& target =
            streamKind == MediaStreamKind::Video ? m_video : m_audio;
        target = std::move(observed);
        return ::media::Status::success();
    }

    std::optional<StreamState>& target =
        streamKind == MediaStreamKind::Video ? m_video : m_audio;
    if (target && target->evidence.generation != evidence.generation) {
        clear(false);
    }
    target = std::move(observed);

    if (!m_video || !m_audio) {
        m_reacquireRequired = false;
        return ::media::Status::success();
    }
    if (m_config.requireMatchingCname &&
        m_video->evidence.cname != m_audio->evidence.cname) {
        clear(true);
        return invalid("RTP clock group CNAME values do not match exactly");
    }
    const auto skew = initialAcquisitionClockOffsetSkew(
        m_video->evidence, m_video->calibration,
        m_audio->evidence, m_audio->calibration);
    if (!skew) {
        clear(true);
        return invalid(
            "RTP clock group sender report clock-offset arithmetic is not representable");
    }
    if (*skew > m_config.maximumInterStreamClockOffsetSkewNs) {
        clear(true);
        return invalid(
            "RTP clock group sender report clock-offset skew exceeds planner threshold");
    }
    m_reacquireRequired = false;
    return ::media::Status::success();
}

MediaRtpClockGroupSnapshot MediaRtpClockGroupValidator::snapshot(
    std::int64_t observedAtNs)
{
    MediaRtpClockGroupSnapshot result{
        m_reacquireRequired ? MediaRtpClockGroupState::ReacquireRequired
                            : MediaRtpClockGroupState::Acquiring,
        m_groupGeneration,
        std::nullopt};
    if (m_phase == Phase::InitialAcquisition && !m_reacquireRequired) {
        discardExpiredInitialCandidates(observedAtNs);
        result.groupGeneration = 0;
    }
    if (m_reacquireRequired || !m_video || !m_audio) return result;

    const auto age = [observedAtNs](const StreamState& stream) -> std::optional<std::int64_t> {
        if (observedAtNs < stream.evidence.senderReportObservedAtNs) return std::nullopt;
        return observedAtNs - stream.evidence.senderReportObservedAtNs;
    };
    const auto videoAge = age(*m_video);
    const auto audioAge = age(*m_audio);
    const auto cnameAge = [observedAtNs](const StreamState& stream) -> std::optional<std::int64_t> {
        if (observedAtNs < stream.evidence.cnameObservedAtNs) return std::nullopt;
        return observedAtNs - stream.evidence.cnameObservedAtNs;
    };
    const auto videoCnameAge = m_config.requireMatchingCname
        ? cnameAge(*m_video) : std::optional<std::int64_t>(0);
    const auto audioCnameAge = m_config.requireMatchingCname
        ? cnameAge(*m_audio) : std::optional<std::int64_t>(0);
    if (!videoAge || !audioAge || *videoAge > m_config.maximumExtrapolationNs ||
        *audioAge > m_config.maximumExtrapolationNs || !videoCnameAge || !audioCnameAge ||
        *videoCnameAge > m_config.videoCnameTimeoutNs ||
        *audioCnameAge > m_config.audioCnameTimeoutNs) {
        clear(true);
        result.state = MediaRtpClockGroupState::ReacquireRequired;
        result.groupGeneration = m_groupGeneration;
        return result;
    }

    result.state = *videoAge > m_config.senderReportTimeoutNs ||
                           *audioAge > m_config.senderReportTimeoutNs
        ? MediaRtpClockGroupState::Degraded
        : MediaRtpClockGroupState::Locked;
    if (result.state != MediaRtpClockGroupState::Locked) return result;
    if (m_phase == Phase::InitialAcquisition) {
        ++m_groupGeneration;
        m_phase = Phase::ActiveGeneration;
    }
    result.groupGeneration = m_groupGeneration;
    MediaRtpSourceClockCalibration video = m_video->calibration;
    MediaRtpSourceClockCalibration audio = m_audio->calibration;
    video.confidence = MediaRtpSourceClockConfidence::Locked;
    audio.confidence = MediaRtpSourceClockConfidence::Locked;
    if (!m_commonSourceEpoch) {
        m_commonSourceEpoch =
            video.actualSenderReportSourceTime < audio.actualSenderReportSourceTime
            ? video.actualSenderReportSourceTime
            : audio.actualSenderReportSourceTime;
    }
    result.locked = MediaRtpLockedClockGroup{
        *m_commonSourceEpoch,
        m_video->evidence.cname,
        std::move(video),
        std::move(audio)};
    return result;
}

void MediaRtpClockGroupValidator::discardExpiredInitialCandidates(
    std::int64_t observedAtNs) noexcept
{
    if (m_video && !initialCandidateIsFresh(
                       *m_video, observedAtNs, m_config.videoCnameTimeoutNs)) {
        m_video.reset();
    }
    if (m_audio && !initialCandidateIsFresh(
                       *m_audio, observedAtNs, m_config.audioCnameTimeoutNs)) {
        m_audio.reset();
    }
}

bool MediaRtpClockGroupValidator::initialCandidateIsFresh(
    const StreamState& stream,
    std::int64_t observedAtNs,
    std::int64_t cnameTimeoutNs) const noexcept
{
    if (observedAtNs < stream.evidence.senderReportObservedAtNs ||
        observedAtNs < stream.evidence.cnameObservedAtNs) {
        return false;
    }
    return observedAtNs - stream.evidence.senderReportObservedAtNs <=
               m_config.senderReportTimeoutNs &&
           (!m_config.requireMatchingCname ||
            observedAtNs - stream.evidence.cnameObservedAtNs <= cnameTimeoutNs);
}

void MediaRtpClockGroupValidator::invalidate() noexcept
{
    clear(true);
}

void MediaRtpClockGroupValidator::clear(bool requireReacquisition) noexcept
{
    ++m_groupGeneration;
    m_video.reset();
    m_audio.reset();
    m_commonSourceEpoch.reset();
    m_reacquireRequired = requireReacquisition;
}

} // namespace media::ffmpeg::graph
