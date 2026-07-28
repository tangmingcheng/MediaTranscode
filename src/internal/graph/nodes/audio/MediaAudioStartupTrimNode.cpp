#include "internal/graph/nodes/audio/MediaAudioStartupTrimNode.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/buffer/MediaBoundCanonicalAudioBuffer.h"
#include "internal/graph/runtime/buffer/MediaDecodedAudioTrimInputBuffer.h"
#include "internal/graph/sync/MediaCanonicalAudioSamplesBuffer.h"
#include "internal/graph/sync/MediaAudioSampleGrid.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

#include <string>

namespace media::ffmpeg::graph {
namespace {

::media::Result<std::int64_t> epochSourceSample(
    const MediaAudioPlaybackOrigin& origin,
    int sourceSampleRate)
{
    auto grid = MediaAudioSampleGrid::create(sourceSampleRate);
    if (!grid) {
        return ::media::Result<std::int64_t>::failure(grid.error());
    }
    return grid.value().firstSampleAtOrAfter(origin.sourceStart);
}

} // namespace

MediaAudioStartupTrimLineageState::MediaAudioStartupTrimLineageState(
    MediaAudioLineageExecutionMode mode,
    std::size_t capacity) noexcept
    : MediaAudioLineageState(
          mode == MediaAudioLineageExecutionMode::SynchronizedReleasedAudio,
          capacity)
{
}

void MediaAudioStartupTrimLineageState::clearOwnedLineage(
    const MediaAvGenerationPurge&) noexcept
{
    clearOwnedState();
}

void MediaAudioStartupTrimLineageState::resetForLifecycle() noexcept
{
    auto lineageLock = lock();
    clearOwnedState();
    resetLifecycleLineage();
}

void MediaAudioStartupTrimLineageState::clearOwnedState() noexcept
{
    origin.reset();
    releaseTrimConsumed = false;
    remainingTrimSamples = 0;
    waitingForFirstPostTrimSample = false;
    expectedNextSample.reset();
}

MediaAudioStartupTrimNode::MediaAudioStartupTrimNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaAudioStartupTrimLineageState> lineageState)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAudioStartupTrimNode")
    , m_lineageState(std::move(lineageState))
{
}

MediaAudioStartupTrimNode::MediaAudioStartupTrimNode(
    MediaNodeId nodeId,
    MediaAudioPlaybackOrigin origin,
    std::shared_ptr<MediaAudioStartupTrimLineageState> lineageState)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaAudioStartupTrimNode")
    , m_lineageState(std::move(lineageState))
{
    m_lineageState->origin = origin;
}

std::string_view MediaAudioStartupTrimNode::generationPurgeIdentity() noexcept
{
    return MediaAudioStartupTrimLineageIdentity;
}
std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaAudioStartupTrimNode::generationPurgeTarget() const noexcept
{
    return m_lineageState->synchronized() ? m_lineageState : nullptr;
}
bool MediaAudioStartupTrimNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto* bound = dynamic_cast<const MediaBoundCanonicalAudioBuffer*>(buffer.get());
    return m_lineageState && m_lineageState->pendingOutputIsCurrent(
        buffer, bound ? std::optional<std::uint64_t>(
                            bound->audioOrigin().generation)
                      : std::nullopt);
}

MediaNodeKind MediaAudioStartupTrimNode::staticKind() noexcept
{
    return MediaNodeKind::AudioStartupTrim;
}

::media::Status MediaAudioStartupTrimNode::start(
    MediaGraphExecutionContext& context)
{
    m_lineageState->resetForLifecycle();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaAudioStartupTrimNode::stop(
    MediaGraphExecutionContext& context)
{
    auto status = FFmpegNodeRuntime::stop(context);
    m_lineageState->resetForLifecycle();
    return status;
}

void MediaAudioStartupTrimNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    FFmpegNodeRuntime::abort(context);
    m_lineageState->resetForLifecycle();
}

::media::Result<MediaBufferRef> MediaAudioStartupTrimNode::validateAndPass(
    const MediaBufferRef& frame,
    bool verifySourceStart)
{
    const auto* canonical = dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
        frame.get());
    if (!canonical || !canonical->lineage() ||
        !m_lineageState->origin ||
        canonical->lineage()->generation != m_lineageState->origin->generation) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup trim requires canonical audio from the active generation"));
    }
    auto epochSample = epochSourceSample(
        *m_lineageState->origin, canonical->interval().sampleRate);
    if (!epochSample ||
        (verifySourceStart &&
         canonical->interval().begin != epochSample.value())) {
        const std::string detail = !epochSample
            ? epochSample.error().message
            : " expected=" + std::to_string(epochSample.value()) +
                  " actual=" +
                  std::to_string(canonical->interval().begin);
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup trim first output sample must equal the epoch "
                "sample-grid boundary:" +
                detail));
    }
    return ::media::Result<MediaBufferRef>::success(frame);
}

::media::Result<MediaBufferRef> MediaAudioStartupTrimNode::apply(
    const MediaBufferRef& frame,
    std::uint32_t trimLeadingSamples)
{
    if (!m_lineageState) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized(
                "Audio startup trim requires planned lineage state"));
    }
    auto lineageLock = m_lineageState->lock();
    const auto* canonical = dynamic_cast<const MediaCanonicalAudioSamplesBuffer*>(
        frame.get());
    const AVFrame* source = canonical ? FFmpegFrameView::frame(canonical->media()) : nullptr;
    if (!canonical || !source || source->nb_samples <= 0 ||
        !m_lineageState->origin ||
        canonical->lineage()->generation != m_lineageState->origin->generation ||
        canonical->interval().end - canonical->interval().begin != source->nb_samples) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup trim requires one exact canonical decoded frame"));
    }
    const bool firstEnvelope = !m_lineageState->releaseTrimConsumed;
    if (!firstEnvelope && trimLeadingSamples != 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup release trim directive can appear exactly once"));
    }
    if (!firstEnvelope &&
        (!m_lineageState->expectedNextSample ||
         canonical->interval().begin != *m_lineageState->expectedNextSample)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup trim requires contiguous continuation samples"));
    }
    if (firstEnvelope) {
        m_lineageState->releaseTrimConsumed = true;
        m_lineageState->remainingTrimSamples = trimLeadingSamples;
    }

    const auto samples = static_cast<std::uint32_t>(source->nb_samples);
    if (m_lineageState->remainingTrimSamples >= samples) {
        m_lineageState->remainingTrimSamples -= samples;
        m_lineageState->waitingForFirstPostTrimSample = true;
        m_lineageState->expectedNextSample = canonical->interval().end;
        return ::media::Result<MediaBufferRef>::success({});
    }
    const auto trimForFrame = m_lineageState->remainingTrimSamples;
    const bool verifySourceStart = firstEnvelope ||
        m_lineageState->waitingForFirstPostTrimSample;
    m_lineageState->remainingTrimSamples = 0;

    if (trimForFrame == 0) {
        auto passed = validateAndPass(frame, verifySourceStart);
        if (passed) {
            m_lineageState->waitingForFirstPostTrimSample = false;
            m_lineageState->expectedNextSample = canonical->interval().end;
        }
        return passed;
    }

    auto output = ::media::ffmpeg::makeFrame();
    if (!output) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::allocationFailed("Audio startup trim could not allocate a frame"));
    }
    output->format = source->format;
    output->sample_rate = source->sample_rate;
    output->nb_samples = source->nb_samples - static_cast<int>(trimForFrame);
    if (av_channel_layout_copy(&output->ch_layout, &source->ch_layout) < 0 ||
        av_frame_get_buffer(output.get(), 0) < 0 ||
        av_samples_copy(output->extended_data, source->extended_data, 0,
                        static_cast<int>(trimForFrame), output->nb_samples,
                        source->ch_layout.nb_channels,
                        static_cast<AVSampleFormat>(source->format)) < 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::ffmpegFailure("Audio startup trim could not copy samples"));
    }
    const int retainedSamples = output->nb_samples;
    auto wrapped = FFmpegBufferFactory::wrapFrame(
        std::move(output), MediaStreamKind::Audio);
    if (!wrapped) return wrapped;
    const auto interval = canonical->interval();
    MediaAudioIntervalAccumulator fragments;
    for (const auto& fragment : canonical->fragments()) {
        if (auto status = fragments.push(fragment); !status) {
            return ::media::Result<MediaBufferRef>::failure(status.error());
        }
    }
    auto discarded = fragments.take(static_cast<int>(trimForFrame));
    if (!discarded) {
        return ::media::Result<MediaBufferRef>::failure(discarded.error());
    }
    auto retained = fragments.take(retainedSamples);
    if (!retained) {
        return ::media::Result<MediaBufferRef>::failure(retained.error());
    }
    auto trimmed = MediaCanonicalAudioSamplesBuffer::create(
        std::move(wrapped).value(), std::move(retained).value());
    if (!trimmed) return trimmed;
    auto passed = validateAndPass(trimmed.value(), true);
    if (passed) {
        m_lineageState->waitingForFirstPostTrimSample = false;
        m_lineageState->expectedNextSample = interval.end;
    }
    return passed;
}

::media::Result<MediaBufferRef> MediaAudioStartupTrimNode::applyDecoded(
    const MediaBufferRef& decodedTrimInput)
{
    auto lineageLock = m_lineageState->lock();
    const auto* decoded = dynamic_cast<const MediaDecodedAudioTrimInputBuffer*>(
        decodedTrimInput.get());
    if (!decoded) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup trim requires an exact decoded origin envelope"));
    }
    if (!m_lineageState->origin) {
        m_lineageState->origin = decoded->audioOrigin();
    }
    if (decoded->audioOrigin() != *m_lineageState->origin) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Audio startup trim rejects a changed active origin"));
    }
    if (auto status = m_lineageState->observe(decoded->audioOrigin().generation); !status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }
    auto trimmed = apply(decoded->media(), decoded->trimLeadingSamples());
    if (!trimmed || !trimmed.value()) return trimmed;
    return MediaBoundCanonicalAudioBuffer::create(
        std::move(trimmed).value(), decoded->audioOrigin());
}

::media::Result<MediaNodeProcessResult> MediaAudioStartupTrimNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "frame");
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) return processWaiting();
    if ((*input.value())->isEof() || (*input.value())->isFlush()) {
        auto lineageLock = m_lineageState->lock();
        if (m_lineageState->remainingTrimSamples != 0 ||
            m_lineageState->waitingForFirstPostTrimSample) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Audio startup trim terminated before the first retained sample"));
        }
        if (auto freshness =
                m_lineageState->authorizeRetainedControl(*input.value());
            !freshness) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                freshness.error());
        }
        auto terminalStatus =
            broadcastControlToAllOutputs(context, *input.value());
        return (*input.value())->isEof()
            ? processFinished(std::move(terminalStatus))
            : processProgress(std::move(terminalStatus));
    }
    auto output = applyDecoded(*input.value());
    if (!output) return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    if (!output.value()) return processProgress();
    return processProgress(emitOutput(context, "frame", output.value()));
}

} // namespace media::ffmpeg::graph
