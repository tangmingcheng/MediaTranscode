#include "internal/graph/nodes/sync/MediaDecodedVideoStartupNode.h"
#include "internal/graph/runtime/buffer/MediaAvStartupEnvelopeBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaVideoDecodeBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/runtime/channel/MediaRequiredInputReader.h"

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
}

MediaDecodedVideoStartupNode::MediaDecodedVideoStartupNode(MediaNodeId id,
    std::shared_ptr<MediaAvSyncGroupRuntime> group,
    std::shared_ptr<MediaPreparationMemoryResource> allocation,
    MediaStartupSharedObjectPlan envelopeStorage,
    std::string sourceIdentity, std::uint64_t initialGeneration)
    : FFmpegNodeRuntime(id, staticKind(), "MediaDecodedVideoStartupNode"),
      m_allocation(std::move(allocation)), m_envelopeStorage(std::move(envelopeStorage)), m_group(std::move(group)), m_sourceIdentity(std::move(sourceIdentity)),
      m_initialGeneration(initialGeneration), m_purge(std::make_shared<MediaOwnerThreadGenerationPurge>()) {}

::media::Status MediaDecodedVideoStartupNode::start(MediaGraphExecutionContext& context)
{
    if (!m_allocation || !m_group || !m_group->clock() || m_initialGeneration == 0 || m_sourceIdentity.empty() ||
        m_group->plan().startup.videoIdentity != std::optional<std::string>{m_sourceIdentity} ||
        m_group->plan().startup.videoReadiness != MediaVideoStartupReadiness::DecodedPresentationSpan)
        return invalid("Decoded startup requires its exact source group, identity, generation and readiness");
    m_lineage.resetGenerationLifecycle();
    if (auto status = m_lineage.observe(m_initialGeneration); !status) return status;
    if (auto status = m_purge->start(context.sharedNodeWakeup(nodeId())); !status) return status;
    auto status = FFmpegNodeRuntime::start(context);
    if (!status) clear();
    return status;
}

void MediaDecodedVideoStartupNode::clear() noexcept
{
    m_purge->stop();
    m_lineage.resetGenerationLifecycle();
}
::media::Status MediaDecodedVideoStartupNode::stop(MediaGraphExecutionContext& context)
{
    auto status = FFmpegNodeRuntime::stop(context);
    clear();
    return status;
}
void MediaDecodedVideoStartupNode::abort(MediaGraphExecutionContext& context) noexcept
{
    FFmpegNodeRuntime::abort(context);
    clear();
}

bool MediaDecodedVideoStartupNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{
    const auto* envelope = dynamic_cast<const MediaAvStartupEnvelopeBuffer*>(buffer.get());
    return m_lineage.pendingOutputIsCurrent(buffer,
        envelope ? std::optional{envelope->unit().generation} : std::nullopt);
}

::media::Result<MediaNodeProcessResult> MediaDecodedVideoStartupNode::process(MediaGraphExecutionContext& context)
{
    if (const auto request = m_purge->pending()) {
        auto status = m_lineage.purge(*request);
        if (status) cancelPendingOutputTransfer();
        auto completed = m_purge->complete(*request, status);
        if (!completed) return processProgress(completed);
        if (!status) return processProgress(status);
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaNodeProcessResult> MediaDecodedVideoStartupNode::onProcess(MediaGraphExecutionContext& context)
{
    auto input = tryReadRequiredInput(context.findInputChannel(nodeId(), "frame"),
        "Decoded video startup", "frame");
    if (!input) return processProgress(::media::Status::failure(input.error()));
    if (!input.value()) return processWaiting();
    auto media = std::move(*input.value());
    if (MediaRetainedControlFreshness::isCandidate(media)) {
        const auto* control = dynamic_cast<const MediaControlBuffer*>(media.get());
        const auto flow = control ? control->flow() : std::nullopt;
        if (!flow) return processProgress(invalid("Decoded startup received invalid frame control"));
        if (control->generation()) {
            auto disposition = m_lineage.classifyObservation(*control->generation());
            if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
            if (disposition.value() == MediaVideoLineageGenerationDisposition::DropStale) return processProgress();
        }
        if (auto status = m_lineage.authorizeRetainedControl(media); !status) return processProgress(status);
        auto status = emitOutput(context, "out", media);
        return *flow == MediaControlBufferFlow::Terminal ? processFinished(status) : processProgress(status);
    }
    const auto* decoded = dynamic_cast<const MediaVideoDecodeBuffer*>(media.get());
    const auto* frame = FFmpegFrameView::frame(media);
    const auto display = decoded ? decoded->displayLineage() : nullptr;
    if (!frame || !display || !display->accessUnit ||
        display->accessUnit->identity.sourceIdentity.view() != m_sourceIdentity ||
        display->accessUnit->mappingConfidence != MediaTimeMappingConfidence::Locked)
        return processProgress(invalid("Decoded startup requires real frame display evidence for its source"));
    const auto generation = display->accessUnit->generation;
    auto disposition = m_lineage.classifyObservation(generation);
    if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
    if (disposition.value() == MediaVideoLineageGenerationDisposition::DropStale) return processProgress();
    if (auto status = m_lineage.observe(generation); !status) return processProgress(status);
    auto now = m_group->clock()->now();
    if (!now) return processProgress(::media::Status::failure(now.error()));
    MediaAvStartupAccessUnit unit{MediaAvStartupStream::Video,
        display->accessUnit->identity.sourceIdentity,
        display->decodedSequence, 0, display->presentation, MediaAvStartupDisplayPoint{},
        MediaSourceClockReadiness::Locked, generation, (frame->flags & AV_FRAME_FLAG_KEY) != 0, std::nullopt};
    auto envelope = MediaAvStartupEnvelopeBuffer::create(m_envelopeStorage, std::move(media), std::move(unit), now.value());
    if (!envelope) return processProgress(::media::Status::failure(envelope.error()));
    return processProgress(emitOutput(context, "out", envelope.value()));
}

} // namespace media::ffmpeg::graph
