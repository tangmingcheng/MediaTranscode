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

::media::Status MediaRtpClockGroupSnapshot::validateMembers(
    MediaTranscodeStreamSet expected) const
{
    if ((members != MediaTranscodeStreamSet::VideoOnly &&
         members != MediaTranscodeStreamSet::AudioVideo) || members != expected ||
        (state != MediaRtpClockGroupState::Acquiring &&
         state != MediaRtpClockGroupState::Locked &&
         state != MediaRtpClockGroupState::Degraded &&
         state != MediaRtpClockGroupState::ReacquireRequired) ||
        (state == MediaRtpClockGroupState::Locked) != locked.has_value() ||
        (locked && (groupGeneration == 0 ||
            (members == MediaTranscodeStreamSet::AudioVideo) != locked->audio.has_value())))
        return invalid("RTP clock snapshot does not match its planned members and state");
    return ::media::Status::success();
}

::media::Result<MediaRtpClockGroupValidator> MediaRtpClockGroupValidator::create(
    MediaRtpClockGroupValidatorConfig config)
{
    if ((config.members != MediaTranscodeStreamSet::VideoOnly &&
         config.members != MediaTranscodeStreamSet::AudioVideo) ||
        (config.members == MediaTranscodeStreamSet::AudioVideo) != config.audio.has_value() ||
        config.senderReportTimeoutNs <= 0 ||
        config.maximumExtrapolationNs <= config.senderReportTimeoutNs ||
        config.videoCnameTimeoutNs <= 0 ||
        (config.audio && (config.audio->cnameTimeoutNs <= 0 ||
                          config.audio->maximumClockOffsetSkewNs <= 0)) ||
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
    if (m_phase == Phase::Exhausted) {
        return invalid("RTP clock group generation is exhausted");
    }
    if ((streamKind != MediaStreamKind::Video && streamKind != MediaStreamKind::Audio) ||
        (streamKind == MediaStreamKind::Audio && !m_config.audio) ||
        (m_config.requireMatchingCname && evidence.cname.empty()) ||
        evidence.senderReportObservedAtNs < 0 ||
        evidence.cnameObservedAtNs < 0 || !matchingCalibration(evidence, calibration)) {
        clear(true);
        return invalid("RTP clock group evidence identity is invalid");
    }

    const auto& previous = streamKind == MediaStreamKind::Video ? m_video : m_audio;
    const bool fresh = !previous || previous->evidence.generation != evidence.generation ||
        previous->evidence.ntp != evidence.ntp ||
        previous->evidence.cname != evidence.cname;
    if (fresh) {
        if (m_evidenceRevision == std::numeric_limits<std::uint64_t>::max())
            return invalid("RTP clock evidence revision exhausted");
        ++m_evidenceRevision;
    }
    StreamState observed{evidence, std::move(calibration)};
    if (m_phase == Phase::ActiveGeneration && m_video &&
        (!m_config.audio || m_audio) &&
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

    if (!m_video || (m_config.audio && !m_audio)) {
        m_reacquireRequired = false;
        return ::media::Status::success();
    }
    if (m_audio) {
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
        if (*skew > m_config.audio->maximumClockOffsetSkewNs) {
            clear(true);
            return invalid(
                "RTP clock group sender report clock-offset skew exceeds planner threshold");
        }
    }
    m_reacquireRequired = false;
    return ::media::Status::success();
}

::media::Result<MediaRtpClockGroupSnapshot> MediaRtpClockGroupValidator::snapshot(
    std::int64_t observedAtNs)
{
    using Result = ::media::Result<MediaRtpClockGroupSnapshot>;
    if (m_phase == Phase::Exhausted) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP clock group generation is exhausted"));
    }
    MediaRtpClockGroupSnapshot result{
        m_config.members,
        m_reacquireRequired ? MediaRtpClockGroupState::ReacquireRequired
                            : MediaRtpClockGroupState::Acquiring,
        m_groupGeneration,
        std::nullopt,
        m_invalidatedGeneration, m_evidenceRevision};
    if (m_phase != Phase::ActiveGeneration && !m_reacquireRequired) {
        discardExpiredAcquisitionCandidates(observedAtNs);
    }
    if (m_reacquireRequired || !m_video || (m_config.audio && !m_audio))
        return Result::success(std::move(result));

    const auto invalidStream = [&](const StreamState& stream, std::int64_t cnameTimeout) {
        if (observedAtNs < stream.evidence.senderReportObservedAtNs ||
            (m_config.requireMatchingCname &&
             observedAtNs < stream.evidence.cnameObservedAtNs)) return true;
        const auto age = observedAtNs - stream.evidence.senderReportObservedAtNs;
        return age > m_config.maximumExtrapolationNs ||
            (m_config.requireMatchingCname &&
             observedAtNs - stream.evidence.cnameObservedAtNs > cnameTimeout) ||
            (m_config.invalidateOnDegraded && age > m_config.senderReportTimeoutNs);
    };
    if (invalidStream(*m_video, m_config.videoCnameTimeoutNs) ||
        (m_audio && invalidStream(*m_audio, m_config.audio->cnameTimeoutNs))) {
        clear(true);
        if (m_phase == Phase::Exhausted) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "RTP clock group generation is exhausted"));
        }
        result.state = m_reacquireRequired
            ? MediaRtpClockGroupState::ReacquireRequired
            : MediaRtpClockGroupState::Acquiring;
        result.groupGeneration = m_groupGeneration;
        result.invalidatedGeneration = m_invalidatedGeneration;
        return Result::success(std::move(result));
    }

    result.state = observedAtNs - m_video->evidence.senderReportObservedAtNs >
                           m_config.senderReportTimeoutNs ||
                       (m_audio && observedAtNs - m_audio->evidence.senderReportObservedAtNs >
                           m_config.senderReportTimeoutNs)
        ? MediaRtpClockGroupState::Degraded
        : MediaRtpClockGroupState::Locked;
    if (result.state != MediaRtpClockGroupState::Locked)
        return Result::success(std::move(result));
    if (m_phase == Phase::InitialAcquisition) {
        ++m_groupGeneration;
    }
    m_phase = Phase::ActiveGeneration;
    m_invalidatedGeneration.reset();
    result.invalidatedGeneration.reset();
    result.groupGeneration = m_groupGeneration;
    MediaRtpSourceClockCalibration video = m_video->calibration;
    video.confidence = MediaRtpSourceClockConfidence::Locked;
    std::optional<MediaRtpSourceClockCalibration> audio;
    if (m_audio) {
        audio = m_audio->calibration;
        audio->confidence = MediaRtpSourceClockConfidence::Locked;
    }
    if (!m_commonSourceEpoch) {
        m_commonSourceEpoch = audio &&
                audio->actualSenderReportSourceTime < video.actualSenderReportSourceTime
            ? audio->actualSenderReportSourceTime : video.actualSenderReportSourceTime;
    }
    result.locked = MediaRtpLockedClockGroup{
        *m_commonSourceEpoch, m_video->evidence.cname,
        std::move(video), std::move(audio)};
    return Result::success(std::move(result));
}

void MediaRtpClockGroupValidator::discardExpiredAcquisitionCandidates(
    std::int64_t observedAtNs) noexcept
{
    if (m_video && !acquisitionCandidateIsFresh(
                       *m_video, observedAtNs, m_config.videoCnameTimeoutNs)) {
        m_video.reset();
    }
    if (m_audio && !acquisitionCandidateIsFresh(
                       *m_audio, observedAtNs, m_config.audio->cnameTimeoutNs)) {
        m_audio.reset();
    }
}

bool MediaRtpClockGroupValidator::acquisitionCandidateIsFresh(
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
    clear(m_phase != Phase::InitialAcquisition);
}

void MediaRtpClockGroupValidator::clear(bool requireReacquisition) noexcept
{
    if (m_phase == Phase::ActiveGeneration) {
        if (m_groupGeneration == (std::numeric_limits<std::uint64_t>::max)()) {
            m_phase = Phase::Exhausted;
            return;
        }
        m_invalidatedGeneration = m_groupGeneration;
        ++m_groupGeneration;
        m_phase = Phase::Reacquiring;
    }
    m_video.reset();
    m_audio.reset();
    m_commonSourceEpoch.reset();
    m_reacquireRequired = requireReacquisition && m_invalidatedGeneration.has_value();
}

} // namespace media::ffmpeg::graph
