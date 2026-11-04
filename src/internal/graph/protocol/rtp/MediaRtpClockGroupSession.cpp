#include "internal/graph/protocol/rtp/MediaRtpClockGroupSession.h"
#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {

MediaRtpClockGroupSession::MediaRtpClockGroupSession(
    MediaRtpClockGroupSessionConfig config, MediaRtpClockGroupValidator validator)
    : m_videoConfig(config.video), m_audioConfig(config.audio),
      m_validator(std::make_unique<MediaRtpClockGroupValidator>(std::move(validator))) {}

::media::Result<MediaRtpClockGroupSession> MediaRtpClockGroupSession::create(
    MediaRtpClockGroupSessionConfig config)
{
    using Result = ::media::Result<MediaRtpClockGroupSession>;
    if (config.audio.has_value() != config.validator.audio.has_value())
        return Result::failure(::media::ErrorInfo::invalidArgument("Clock group session member configuration mismatch"));
    if (auto status = MediaRtpSourceClockMapper::validateConfig(config.video); !status)
        return Result::failure(status.error());
    if (config.audio) {
        if (auto status = MediaRtpSourceClockMapper::validateConfig(*config.audio); !status)
            return Result::failure(status.error());
    }
    const auto matches = [&](const MediaRtpSourceClockMapperConfig& mapper) {
        return mapper.requireCname == config.validator.requireMatchingCname &&
            mapper.senderReportTimeoutNs == config.validator.senderReportTimeoutNs &&
            mapper.maximumExtrapolationNs == config.validator.maximumExtrapolationNs;
    };
    if (!matches(config.video) || (config.audio && !matches(*config.audio)))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Clock group session mapper and validator facts must agree"));
    auto validator = MediaRtpClockGroupValidator::create(config.validator);
    if (!validator) return Result::failure(validator.error());
    return Result::success(MediaRtpClockGroupSession(std::move(config), std::move(validator).value()));
}

::media::Result<MediaRtpClockGroupConsumption> MediaRtpClockGroupSession::consume(
    const MediaRtpIngressEventView& envelope, MediaStreamKind streamKind,
    bool selectedClock, std::int64_t nowNs)
{
    using Result = ::media::Result<MediaRtpClockGroupConsumption>;
    if ((streamKind != MediaStreamKind::Video && streamKind != MediaStreamKind::Audio) ||
        (streamKind == MediaStreamKind::Audio && !m_audioConfig))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Clock group session received an unplanned stream member"));
    const auto* event = &envelope;
    auto& lastSequence = streamKind == MediaStreamKind::Video ? m_videoSequence : m_audioSequence;
    if (event->sequence == 0 ||
        (lastSequence &&
         event->sequence <= *lastSequence)) {
        return Result::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode requires strictly ordered ingress sequence"));
    }
    lastSequence = event->sequence;
    ::media::Status status = ::media::Status::success();
    std::int64_t observationTimeNs = nowNs;
    if (selectedClock) {
        status = processClock(*event, streamKind);
    } else if (event->kind == MediaRtpIngressEventKind::ClockObservation &&
               event->clockObservation) {
        observationTimeNs = event->clockObservation->observedAtNs;
        if (observationTimeNs < 0) {
            return Result::failure(
                ::media::ErrorInfo::invalidArgument(
                    "MediaRtpClockGroupNode clock observation time must be non-negative"));
        }
    } else if (event->kind == MediaRtpIngressEventKind::ClockInvalidation &&
               event->clockInvalidation) {
        auto& minimumGeneration = streamKind == MediaStreamKind::Video
            ? m_minimumVideoGeneration
            : m_minimumAudioGeneration;
        const auto& invalidation = *event->clockInvalidation;
        if (invalidation.observedAtNs < 0)
            return Result::failure(
                ::media::ErrorInfo::invalidArgument("RTP invalidation requires monotonic observation time"));
        const std::uint64_t invalidatedGeneration = invalidation.generation;
        minimumGeneration = minimumGeneration
            ? std::max(*minimumGeneration, invalidatedGeneration)
            : invalidatedGeneration;
        m_validator->invalidate();
        m_videoMapper.reset();
        m_audioMapper.reset();
        m_videoGeneration.reset();
        m_audioGeneration.reset();
    } else if (event->kind == MediaRtpIngressEventKind::Discontinuity &&
               event->discontinuity && event->discontinuityGeneration) {
        auto& minimumGeneration = streamKind == MediaStreamKind::Video
            ? m_minimumVideoGeneration
            : m_minimumAudioGeneration;
        const std::uint64_t invalidatedGeneration = *event->discontinuityGeneration;
        minimumGeneration = minimumGeneration
            ? std::max(*minimumGeneration, invalidatedGeneration)
            : invalidatedGeneration;
        m_validator->invalidate();
        m_videoMapper.reset();
        m_audioMapper.reset();
        m_videoGeneration.reset();
        m_audioGeneration.reset();
    } else {
        status = ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtpClockGroupNode event port requires discontinuity"));
    }
    if (!status) m_validator->invalidate();
    return Result::success({observationTimeNs,
        status ? std::nullopt : std::optional<::media::ErrorInfo>{status.error()}});
}

::media::Status MediaRtpClockGroupSession::processClock(
    const MediaRtpIngressEventView& event,
    MediaStreamKind streamKind)
{
    if (event.kind != MediaRtpIngressEventKind::ClockEvidence || !event.clockEvidence) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("MediaRtpClockGroupNode clock port requires clock evidence"));
    }
    const MediaRtcpClockEvidence& evidence = *event.clockEvidence;
    const auto& minimumGeneration = streamKind == MediaStreamKind::Video
        ? m_minimumVideoGeneration
        : m_minimumAudioGeneration;
    if (minimumGeneration && evidence.generation < *minimumGeneration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MediaRtpClockGroupNode rejects clock evidence from an invalidated generation"));
    }
    auto& mapper = streamKind == MediaStreamKind::Video ? m_videoMapper : m_audioMapper;
    auto& generation = streamKind == MediaStreamKind::Video ? m_videoGeneration : m_audioGeneration;
    if (streamKind == MediaStreamKind::Audio && !m_audioConfig)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RTP clock group received an unplanned audio member"));
    const auto& config = streamKind == MediaStreamKind::Video ? m_videoConfig : *m_audioConfig;
    if (!mapper || !generation || *generation != evidence.generation) {
        auto created = MediaRtpSourceClockMapper::create(config, evidence.generation);
        if (!created) return ::media::Status::failure(created.error());
        mapper = std::make_unique<MediaRtpSourceClockMapper>(std::move(created).value());
        generation = evidence.generation;
    }
    auto observed = mapper->observeSenderReport(evidence);
    if (!observed) return observed;
    auto calibration = mapper->calibration(evidence.senderReportObservedAtNs);
    if (!calibration) return ::media::Status::failure(calibration.error());
    auto status = m_validator->observe(
        streamKind, evidence, std::move(calibration).value());
    return status;
}

::media::Result<MediaRtpClockGroupSnapshot> MediaRtpClockGroupSession::snapshot(std::int64_t observedAtNs)
{
    return m_validator->snapshot(observedAtNs);
}

} // namespace media::ffmpeg::graph
