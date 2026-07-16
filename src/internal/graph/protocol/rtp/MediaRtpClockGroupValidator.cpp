#include "internal/graph/protocol/rtp/MediaRtpClockGroupValidator.h"

#include <cstdlib>
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

} // namespace

::media::Result<MediaRtpClockGroupValidator> MediaRtpClockGroupValidator::create(
    MediaRtpClockGroupValidatorConfig config)
{
    if (config.senderReportTimeoutNs <= 0 ||
        config.maximumExtrapolationNs <= config.senderReportTimeoutNs ||
        config.maximumInterStreamSkewNs <= 0 || config.videoCnameTimeoutNs <= 0 ||
        config.audioCnameTimeoutNs <= 0) {
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
        evidence.cname.empty() || evidence.senderReportObservedAtNs < 0 ||
        evidence.cnameObservedAtNs < 0 || !matchingCalibration(evidence, calibration)) {
        clear(true);
        return invalid("RTP clock group evidence identity is invalid");
    }

    std::optional<StreamState>& target =
        streamKind == MediaStreamKind::Video ? m_video : m_audio;
    if (target && target->evidence.generation != evidence.generation) {
        clear(false);
    }
    target = StreamState{evidence, std::move(calibration)};

    if (!m_video || !m_audio) {
        m_reacquireRequired = false;
        return ::media::Status::success();
    }
    if (m_video->evidence.cname != m_audio->evidence.cname) {
        clear(true);
        return invalid("RTP clock group CNAME values do not match exactly");
    }
    const std::int64_t skew = std::llabs(
        m_video->calibration.actualSenderReportSourceTime.nanoseconds() -
        m_audio->calibration.actualSenderReportSourceTime.nanoseconds());
    if (skew > m_config.maximumInterStreamSkewNs) {
        clear(true);
        return invalid("RTP clock group sender report skew exceeds planner threshold");
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
    const auto videoCnameAge = cnameAge(*m_video);
    const auto audioCnameAge = cnameAge(*m_audio);
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
    if (!m_everLocked) {
        ++m_groupGeneration;
        m_everLocked = true;
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
