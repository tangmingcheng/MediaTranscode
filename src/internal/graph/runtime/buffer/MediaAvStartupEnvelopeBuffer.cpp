#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"

#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaAvStartupEnvelopeBuffer::create(
    MediaBufferRef media,
    MediaAvStartupAccessUnit unit,
    MediaRunningTime observedAt)
{
    if (!media) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("A/V startup envelope requires media"));
    }
    const auto footprint = media->payloadFootprintBytes();
    if (!footprint || *footprint == 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup envelope requires a trusted non-zero payload footprint"));
    }
    unit.payloadBytes = *footprint;
    if (unit.stream == MediaAvStartupStream::Audio) {
        if (!unit.audio) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V startup audio envelope requires a sample span"));
        }
        if (auto status = validateMediaAvAudioSampleSpanDuration(
                *unit.audio, unit.duration); !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
    }
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(new MediaAvStartupEnvelopeBuffer(
            std::move(media), std::move(unit), observedAt)));
}

MediaAvStartupEnvelopeBuffer::MediaAvStartupEnvelopeBuffer(
    MediaBufferRef media,
    MediaAvStartupAccessUnit unit,
    MediaRunningTime observedAt)
    : m_media(std::move(media))
    , m_unit(std::move(unit))
    , m_observedAt(observedAt)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("av_startup.canonical_envelope");
}

MediaBufferType MediaAvStartupEnvelopeBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

const MediaBufferRef& MediaAvStartupEnvelopeBuffer::media() const noexcept { return m_media; }
const MediaAvStartupAccessUnit& MediaAvStartupEnvelopeBuffer::unit() const noexcept { return m_unit; }
MediaRunningTime MediaAvStartupEnvelopeBuffer::observedAt() const noexcept
{
    return m_observedAt;
}

MediaAvStartupClockBuffer::MediaAvStartupClockBuffer(MediaRunningTime masterNow)
    : m_masterNow(masterNow)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("av_startup.master_clock_tick");
}

MediaBufferType MediaAvStartupClockBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

MediaRunningTime MediaAvStartupClockBuffer::masterNow() const noexcept { return m_masterNow; }

MediaAvStartupReleaseBuffer::MediaAvStartupReleaseBuffer(
    MediaAvSyncGroupKey groupKey,
    MediaAvStartupReleaseKind releaseKind,
    MediaPlaybackActivation activation,
    std::vector<MediaAvReleasedUnit> video,
    std::vector<MediaAvReleasedUnit> audio,
    std::optional<std::uint64_t> completedTransitionSequence)
    : m_groupKey(std::move(groupKey))
    , m_releaseKind(releaseKind)
    , m_activation(std::move(activation))
    , m_video(std::move(video))
    , m_audio(std::move(audio))
    , m_completedTransitionSequence(completedTransitionSequence)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName("av_startup.atomic_release");
}

::media::Result<MediaBufferRef> MediaAvStartupReleaseBuffer::create(
    MediaAvSyncGroupKey groupKey,
    MediaAvStartupReleaseKind releaseKind,
    MediaPlaybackActivation activation,
    std::vector<MediaAvReleasedUnit> video,
    std::vector<MediaAvReleasedUnit> audio,
    std::optional<std::uint64_t> completedTransitionSequence)
{
    if (auto status = validateReleaseKind(releaseKind); !status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    bool shapeValid = false;
    switch (releaseKind) {
    case MediaAvStartupReleaseKind::InitialAtomicRelease:
        shapeValid = !video.empty() && (activation.members() == MediaTranscodeStreamSet::VideoOnly
            ? audio.empty() : !audio.empty());
        break;
    case MediaAvStartupReleaseKind::ActiveEpochPassThrough:
        shapeValid = (!video.empty() || !audio.empty()) &&
            (activation.members() == MediaTranscodeStreamSet::AudioVideo || audio.empty());
        break;
    case MediaAvStartupReleaseKind::NextAtomicRelease:
        shapeValid = !video.empty() && (activation.members() == MediaTranscodeStreamSet::VideoOnly
            ? audio.empty() : !audio.empty());
        break;
    }
    const bool transitionSequenceValid =
        releaseKind == MediaAvStartupReleaseKind::NextAtomicRelease
        ? completedTransitionSequence.has_value() &&
              *completedTransitionSequence != 0
        : !completedTransitionSequence.has_value();
    if (!groupKey.valid() || !shapeValid ||
        !transitionSequenceValid) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup release contract is incomplete"));
    }
    const auto hasNull = [](const auto& units) {
        for (const auto& unit : units) if (!unit.media) return true;
        return false;
    };
    if (hasNull(video) || hasNull(audio)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup release contains null media"));
    }
    return ::media::Result<MediaBufferRef>::success(MediaBufferRef(
        new MediaAvStartupReleaseBuffer(
            std::move(groupKey), releaseKind, std::move(activation),
            std::move(video), std::move(audio),
            completedTransitionSequence)));
}

::media::Status MediaAvStartupReleaseBuffer::validateReleaseKind(
    MediaAvStartupReleaseKind releaseKind) noexcept
{
    switch (releaseKind) {
    case MediaAvStartupReleaseKind::InitialAtomicRelease:
    case MediaAvStartupReleaseKind::ActiveEpochPassThrough:
    case MediaAvStartupReleaseKind::NextAtomicRelease:
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "A/V startup release kind is unsupported"));
}

MediaBufferType MediaAvStartupReleaseBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

const MediaPlaybackEpoch& MediaAvStartupReleaseBuffer::epoch() const noexcept { return m_activation.epoch(); }
const MediaAvSyncGroupKey& MediaAvStartupReleaseBuffer::groupKey() const noexcept { return m_groupKey; }
MediaAvStartupReleaseKind MediaAvStartupReleaseBuffer::releaseKind() const noexcept { return m_releaseKind; }
const MediaPlaybackActivation& MediaAvStartupReleaseBuffer::activation() const noexcept { return m_activation; }
const std::vector<MediaAvReleasedUnit>& MediaAvStartupReleaseBuffer::video() const noexcept { return m_video; }
const std::vector<MediaAvReleasedUnit>& MediaAvStartupReleaseBuffer::audio() const noexcept { return m_audio; }
const std::optional<std::uint64_t>&
MediaAvStartupReleaseBuffer::completedTransitionSequence() const noexcept
{
    return m_completedTransitionSequence;
}

} // namespace media::ffmpeg::graph
