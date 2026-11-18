#include "internal/graph/sync/MediaScheduledAccessUnit.h"

#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/diagnostics/MediaAccessUnitProcessingTrace.h"

#include <utility>

namespace media::ffmpeg::graph {

MediaScheduledAccessUnitParameters::MediaScheduledAccessUnitParameters(
    MediaBufferRef mediaValue,
    MediaScheduledStream streamValue,
    MediaRunningTime canonicalPresentationValue,
    MediaRunningTime canonicalDispatchValue,
    MediaRunningTime presentationOnMasterValue,
    MediaRunningTime dispatchOnMasterValue,
    MediaRunningTime emitOnMasterValue,
    MediaRunningTime canonicalDurationValue,
    std::uint64_t generationValue,
    MediaCanonicalAccessUnitSequence canonicalSequenceValue,
    std::optional<MediaCodecReceiveObservation> codecReceiveObservationValue)
    : media(std::move(mediaValue)), stream(streamValue),
      canonicalPresentation(canonicalPresentationValue),
      canonicalDispatch(canonicalDispatchValue),
      presentationOnMaster(presentationOnMasterValue),
      dispatchOnMaster(dispatchOnMasterValue),
      emitOnMaster(emitOnMasterValue),
      canonicalDuration(canonicalDurationValue), generation(generationValue),
      canonicalSequence(canonicalSequenceValue), codecReceiveObservation(codecReceiveObservationValue)
{
}

::media::Result<MediaBufferRef> MediaScheduledAccessUnit::create(
    MediaScheduledAccessUnitParameters parameters)
{
    const auto expectedStream = parameters.stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video : MediaStreamKind::Audio;
    if (!FFmpegPacketView::isPacket(parameters.media) ||
        parameters.media->streamKind() != expectedStream ||
        parameters.generation == 0 || parameters.canonicalSequence.value() == 0 ||
        parameters.canonicalDuration.nanoseconds() < 0 ||
        (parameters.stream != MediaScheduledStream::Video && parameters.stream != MediaScheduledStream::Audio)) {
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
      m_emitOnMaster(parameters.emitOnMaster),
      m_canonicalDuration(parameters.canonicalDuration),
      m_generation(parameters.generation),
      m_canonicalSequence(parameters.canonicalSequence),
      m_diagnosticCreatedSteadyNs(mediaAccessUnitObservationNow()),
      m_codecReceiveObservation(parameters.codecReceiveObservation)
{
    setStreamKind(m_stream == MediaScheduledStream::Video
                      ? MediaStreamKind::Video : MediaStreamKind::Audio);
    setPayloadKind(MediaPayloadKind::Packet);
}

MediaBufferType MediaScheduledAccessUnit::type() const noexcept
{
    return MediaBufferType::Event;
}

std::optional<std::uint64_t>
MediaScheduledAccessUnit::payloadFootprintBytes() const noexcept
{
    return m_media->payloadFootprintBytes();
}

} // namespace media::ffmpeg::graph
