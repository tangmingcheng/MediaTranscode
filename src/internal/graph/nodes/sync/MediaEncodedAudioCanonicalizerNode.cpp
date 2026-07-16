#include "internal/graph/nodes/sync/MediaEncodedAudioCanonicalizerNode.h"

#include "internal/graph/runtime/buffer/MediaEncodedAudioLineageBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"
#include "internal/graph/sync/MediaCanonicalLineage.h"
#include "internal/graph/sync/lineage/MediaAudioLineageIdentities.h"
#include "internal/graph/sync/lineage/MediaEncodedAudioCanonicalizerState.h"

namespace media::ffmpeg::graph {

MediaEncodedAudioCanonicalizerNode::MediaEncodedAudioCanonicalizerNode(
    MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(),
                        "MediaEncodedAudioCanonicalizerNode")
    , m_state(std::make_shared<MediaEncodedAudioCanonicalizerState>())
{
}

std::string_view
MediaEncodedAudioCanonicalizerNode::generationPurgeIdentity() noexcept
{
    return MediaEncodedAudioCanonicalizerLineageIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaEncodedAudioCanonicalizerNode::generationPurgeTarget() const noexcept
{
    return m_state;
}

MediaNodeKind MediaEncodedAudioCanonicalizerNode::staticKind() noexcept
{
    return MediaNodeKind::EncodedAudioCanonicalizer;
}

::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>
MediaEncodedAudioCanonicalizerNode::canonicalize(
    const MediaBufferRef& encoded,
    MediaSourceAccessUnitSequence sequence)
{
    const auto* input = dynamic_cast<const MediaEncodedAudioLineageBuffer*>(
        encoded.get());
    if (!input || input->fragments().empty()) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Encoded audio canonicalizer requires exact encoded lineage"));
    }
    const auto& origin = input->audioOrigin();
    const auto& fragments = input->fragments();
    const auto begin = fragments.front().interval.begin;
    const auto end = fragments.back().interval.end;
    if (begin < origin.epochOutputSampleIndex || end <= begin ||
        fragments.front().interval.sampleRate != origin.outputSampleRate) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Encoded audio canonicalizer rejects invalid output interval"));
    }
    auto offset = MediaRunningTime::checkedFromTicks(
        begin - origin.epochOutputSampleIndex, 1, origin.outputSampleRate);
    auto duration = MediaRunningTime::checkedFromTicks(
        end - begin, 1, origin.outputSampleRate);
    if (!offset || !duration) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            !offset ? offset.error() : duration.error());
    }
    auto presentation = origin.sourceStart.checkedAdd(offset.value());
    if (!presentation) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            presentation.error());
    }
    auto lineage = createMediaCanonicalLineage(
        presentation.value(), std::nullopt, duration.value(),
        MediaDecodeOrderMode::PresentationOrderNoReorder,
        std::string(generationPurgeIdentity()), sequence,
        MediaTimeMappingConfidence::Locked,
        origin.generation);
    if (!lineage) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            lineage.error());
    }
    auto canonical = MediaCanonicalAccessUnitBuffer::create(
        input->media(), std::move(lineage).value());
    if (!canonical) {
        return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::failure(
            canonical.error());
    }
    return ::media::Result<std::shared_ptr<MediaCanonicalAccessUnitBuffer>>::success(
        std::dynamic_pointer_cast<MediaCanonicalAccessUnitBuffer>(
            std::move(canonical).value()));
}

::media::Status MediaEncodedAudioCanonicalizerNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaEncodedAudioCanonicalizerNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaEncodedAudioCanonicalizerNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaEncodedAudioCanonicalizerNode::resetState() noexcept
{
    m_state->resetForLifecycle();
}

bool MediaEncodedAudioCanonicalizerNode::pendingOutputIsCurrent(
    const MediaBufferRef& buffer) const noexcept
{
    return m_state->pendingOutputIsCurrent(buffer, std::nullopt);
}

::media::Result<MediaNodeProcessResult>
MediaEncodedAudioCanonicalizerNode::onProcess(
    MediaGraphExecutionContext& context)
{
    auto stateLock = m_state->lock();
    if (!m_state->pending) {
        auto input = tryReadRequiredInput(
            context.findInputChannel(nodeId(), "encoded"),
            "Encoded audio canonicalizer", "encoded");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(input.error());
        }
        if (!input.value()) return processWaiting();
        if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
                input.value()->get())) {
            if (control->controlKind() == MediaControlBufferKind::Unknown) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Encoded audio canonicalizer rejects unknown control"));
            }
            if (auto authorized = m_state->authorizeRetainedControl(
                    *input.value()); !authorized) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    authorized.error());
            }
            auto status = emitOutput(context, "canonical", *input.value());
            return control->controlKind() == MediaControlBufferKind::Eof ||
                           control->controlKind() == MediaControlBufferKind::Abort
                ? processFinished(std::move(status))
                : processProgress(std::move(status));
        }
        const auto* encoded = dynamic_cast<const MediaEncodedAudioLineageBuffer*>(
            input.value()->get());
        if (!encoded || encoded->fragments().empty()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Encoded audio canonicalizer requires lineage media"));
        }
        const auto begin = encoded->fragments().front().interval.begin;
        const auto end = encoded->fragments().back().interval.end;
        if (auto observed = m_state->validateObservation(
                encoded->audioOrigin().generation); !observed) {
            return ::media::Result<MediaNodeProcessResult>::failure(observed.error());
        }
        if (m_state->expectedNextSample &&
            *m_state->expectedNextSample != begin) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Encoded audio canonicalizer rejects interval discontinuity"));
        }
        auto output = canonicalize(
            *input.value(), MediaSourceAccessUnitSequence(m_state->nextSequence));
        if (!output) {
            return ::media::Result<MediaNodeProcessResult>::failure(output.error());
        }
        m_state->pending = MediaEncodedAudioCanonicalizerState::PendingOutput{
            std::move(output).value(), end, encoded->audioOrigin().generation,
            m_state->nextSequence + 1};
    }
    MediaChannel* output = context.findOutputChannel(nodeId(), "canonical");
    if (!output || output->closed() || output->aborted()) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Encoded audio canonicalizer output is unavailable"));
    }
    if (output->policy().queuePolicy.overflowPolicy !=
        MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Encoded audio canonicalizer requires blocking output"));
    }
    if (output->size() >= output->capacity()) return processWaiting();
    if (output->pushOutcome(m_state->pending->output) !=
        MediaQueuePushOutcome::Accepted) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::internalError(
                "Encoded audio canonicalizer commit diverged after preflight"));
    }
    if (auto observed = m_state->observe(m_state->pending->generation);
        !observed) {
        return ::media::Result<MediaNodeProcessResult>::failure(observed.error());
    }
    m_state->expectedNextSample = m_state->pending->nextSample;
    m_state->nextSequence = m_state->pending->nextSequence;
    m_state->pending.reset();
    return processProgress();
}

} // namespace media::ffmpeg::graph
