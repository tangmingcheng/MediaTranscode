#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"

#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/buffer/FFmpegFrameBuffer.h"

#include "internal/graph/planner/realtime/MediaStartupSharedObjectPlanner.h"
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaAvStartupEnvelopeBuffer::create(
    const MediaStartupSharedObjectPlan& storage,
    MediaBufferRef media,
    MediaAvStartupAccessUnit unit,
    MediaRunningTime observedAt)
{
    if (!media) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("A/V startup envelope requires media"));
    }
    if (std::holds_alternative<MediaAvStartupDisplayPoint>(unit.timing)) {
        const auto* decoded = dynamic_cast<const MediaVideoDecodeBuffer*>(media.get());
        const auto* frame = decoded
            ? dynamic_cast<const FFmpegFrameBuffer*>(decoded->media().get()) : nullptr;
        if (unit.stream != MediaAvStartupStream::Video || !decoded || !frame ||
            !frame->frame() || !decoded->lineage() ||
            decoded->decodedSequence() != std::optional<std::uint64_t>{unit.sequence} ||
            !decoded->decodedPresentation() ||
            decoded->decodedPresentation() != unit.presentationTime ||
            decoded->lineage()->identity.sourceIdentity != unit.identity ||
            decoded->lineage()->generation != unit.generation ||
            unit.readiness != MediaSourceClockReadiness::Locked ||
            unit.keyFrame != ((frame->frame()->flags & AV_FRAME_FLAG_KEY) != 0)) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V startup display point requires matching decoded frame evidence"));
        }
    }
    const auto footprint = media->payloadFootprintBytes();
    if (!footprint || *footprint == 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup envelope requires a trusted non-zero payload footprint"));
    }
    unit.payloadBytes = *footprint;
    if (unit.stream == MediaAvStartupStream::Audio) {
        const auto* duration = std::get_if<MediaRunningTime>(&unit.timing);
        if (!unit.audio || !duration) {
            return ::media::Result<MediaBufferRef>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "A/V startup audio envelope requires a sample span"));
        }
        if (auto status = validateMediaAvAudioSampleSpanDuration(
                *unit.audio, *duration); !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
    }
    if (auto valid = MediaStartupSharedObjectPlanner::validate(storage, MediaStartupSharedObjectRole::Envelope); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    try {
        return ::media::Result<MediaBufferRef>::success(
            std::allocate_shared<MediaAvStartupEnvelopeBuffer>(
                AllocationAllocator{MediaLineageAllocator<std::byte>{storage.allocation()}.resource()},
                ConstructionKey{}, std::move(media), std::move(unit), observedAt));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("A/V startup envelope allocation failed"));
    }
}

MediaAvStartupEnvelopeBuffer::MediaAvStartupEnvelopeBuffer(
    ConstructionKey,
    MediaBufferRef media,
    MediaAvStartupAccessUnit unit,
    MediaRunningTime observedAt)
    : m_media(std::move(media))
    , m_unit(std::move(unit))
    , m_observedAt(observedAt)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName(MediaDiagnosticLiteral{"av_startup.canonical_envelope"});
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
    setDiagnosticName(MediaDiagnosticLiteral{"av_startup.master_clock_tick"});
}

MediaBufferType MediaAvStartupClockBuffer::type() const noexcept
{
    return MediaBufferType::Event;
}

MediaRunningTime MediaAvStartupClockBuffer::masterNow() const noexcept { return m_masterNow; }

MediaAvStartupReleaseBuffer::MediaAvStartupReleaseBuffer(
    ConstructionKey,
    MediaStartupSharedObjectPlan storage,
    MediaAvSyncGroupKey groupKey,
    MediaAvStartupReleaseKind releaseKind,
    MediaPlaybackActivation activation,
    MediaAvReleasedUnits video,
    MediaAvReleasedUnits audio,
    std::optional<std::uint64_t> completedTransitionSequence)
    : m_storage(std::move(storage)), m_groupKey(std::move(groupKey))
    , m_releaseKind(releaseKind)
    , m_activation(std::move(activation))
    , m_video(std::move(video))
    , m_audio(std::move(audio))
    , m_completedTransitionSequence(completedTransitionSequence)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::GraphEvent);
    setDiagnosticName(MediaDiagnosticLiteral{"av_startup.atomic_release"});
}

::media::Result<MediaBufferRef> MediaAvStartupReleaseBuffer::create(
    MediaStartupSharedObjectPlan storage,
    MediaAvSyncGroupKey groupKey,
    MediaAvStartupReleaseKind releaseKind,
    MediaPlaybackActivation activation,
    MediaAvReleasedUnits video,
    MediaAvReleasedUnits audio,
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
    if (auto valid = MediaStartupSharedObjectPlanner::validate(storage, MediaStartupSharedObjectRole::Release); !valid)
        return ::media::Result<MediaBufferRef>::failure(valid.error());
    const auto allocator = AllocationAllocator{storage.allocation()};
    if (!allocator.resource() || video.resource() != allocator.resource() ||
        audio.resource() != allocator.resource() ||
        video.size() != video.capacity() || audio.size() != audio.capacity()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup release requires one explicit allocation resource"));
    }
    try {
        return ::media::Result<MediaBufferRef>::success(
            std::allocate_shared<MediaAvStartupReleaseBuffer>(
                allocator, ConstructionKey{}, std::move(storage),
                std::move(groupKey), releaseKind, std::move(activation),
                std::move(video), std::move(audio), completedTransitionSequence));
    } catch (const std::bad_alloc&) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("A/V startup release allocation failed"));
    }
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
const MediaAvReleasedUnits& MediaAvStartupReleaseBuffer::video() const noexcept { return m_video; }
const MediaAvReleasedUnits& MediaAvStartupReleaseBuffer::audio() const noexcept { return m_audio; }
const std::optional<std::uint64_t>&
MediaAvStartupReleaseBuffer::completedTransitionSequence() const noexcept
{
    return m_completedTransitionSequence;
}

} // namespace media::ffmpeg::graph
