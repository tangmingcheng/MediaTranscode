#include "internal/graph/sync/MediaScheduledAccessUnit.h"

#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaScheduledAccessUnitParameters::MediaScheduledAccessUnitParameters(
    MediaBufferRef mediaValue,
    MediaScheduledStream streamValue,
    MediaRunningTime canonicalPresentationValue,
    MediaRunningTime canonicalDispatchValue,
    MediaRunningTime presentationOnMasterValue,
    MediaRunningTime dispatchOnMasterValue,
    MediaRunningTime canonicalDurationValue,
    std::uint64_t generationValue,
    MediaSourceAccessUnitSequence sourceSequenceValue,
    std::optional<MediaSourceAccessUnitSequence> repeatedFromValue,
    std::optional<MediaVideoRepeatRequestId> repeatRequestIdValue,
    std::optional<MediaVideoSyncDecisionKind> videoDecisionValue)
    : media(std::move(mediaValue)), stream(streamValue),
      canonicalPresentation(canonicalPresentationValue),
      canonicalDispatch(canonicalDispatchValue),
      presentationOnMaster(presentationOnMasterValue),
      dispatchOnMaster(dispatchOnMasterValue),
      canonicalDuration(canonicalDurationValue), generation(generationValue),
      sourceSequence(sourceSequenceValue), repeatedFrom(repeatedFromValue),
      repeatRequestId(repeatRequestIdValue), videoDecision(videoDecisionValue)
{
}

::media::Result<MediaBufferRef> MediaScheduledAccessUnit::create(
    MediaScheduledAccessUnitParameters parameters)
{
    const auto expectedStream = parameters.stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video : MediaStreamKind::Audio;
    const bool hasRepeatIdentity = parameters.repeatedFrom.has_value() &&
        parameters.repeatRequestId.has_value() &&
        parameters.repeatRequestId->value() > 0 &&
        parameters.sourceSequence == *parameters.repeatedFrom;
    bool allowedVideoDecision = false;
    if (parameters.videoDecision) {
        switch (*parameters.videoDecision) {
        case MediaVideoSyncDecisionKind::Display:
        case MediaVideoSyncDecisionKind::DisplayLate:
        case MediaVideoSyncDecisionKind::DisplayPreservedKeyFrame:
            allowedVideoDecision = !parameters.repeatedFrom &&
                !parameters.repeatRequestId;
            break;
        case MediaVideoSyncDecisionKind::RepeatPreviousFrame:
            allowedVideoDecision = hasRepeatIdentity;
            break;
        case MediaVideoSyncDecisionKind::Hold:
        case MediaVideoSyncDecisionKind::Drop:
        case MediaVideoSyncDecisionKind::Reacquire:
        case MediaVideoSyncDecisionKind::NoAction:
        case MediaVideoSyncDecisionKind::DropOldGeneration:
            break;
        }
    }
    const bool videoContract = parameters.stream == MediaScheduledStream::Video
        ? allowedVideoDecision
        : !parameters.videoDecision && !parameters.repeatedFrom &&
              !parameters.repeatRequestId;
    if (!FFmpegPacketView::isPacket(parameters.media) ||
        parameters.media->streamKind() != expectedStream ||
        parameters.generation == 0 || parameters.sourceSequence.value() == 0 ||
        parameters.canonicalDuration.nanoseconds() < 0 || !videoContract) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled access unit parameters are incomplete"));
    }
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaScheduledAccessUnit(std::move(parameters))));
}

MediaScheduledAccessUnit::MediaScheduledAccessUnit(
    MediaScheduledAccessUnitParameters parameters)
    : m_media(std::move(parameters.media)), m_stream(parameters.stream),
      m_canonicalPresentation(parameters.canonicalPresentation),
      m_canonicalDispatch(parameters.canonicalDispatch),
      m_presentationOnMaster(parameters.presentationOnMaster),
      m_dispatchOnMaster(parameters.dispatchOnMaster),
      m_canonicalDuration(parameters.canonicalDuration),
      m_generation(parameters.generation),
      m_sourceSequence(parameters.sourceSequence),
      m_repeatedFromSourceSequence(parameters.repeatedFrom),
      m_repeatRequestId(parameters.repeatRequestId),
      m_videoDecision(parameters.videoDecision)
{
    setStreamKind(m_stream == MediaScheduledStream::Video
                      ? MediaStreamKind::Video : MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Packet);
}

MediaBufferType MediaScheduledAccessUnit::type() const noexcept
{
    return MediaBufferType::Event;
}

} // namespace media::ffmpeg::graph
