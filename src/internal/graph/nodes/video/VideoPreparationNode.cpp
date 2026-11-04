#include "internal/graph/model/MediaStartupReservationOwners.h"
#include "internal/graph/nodes/video/VideoPreparationNode.h"
#include "internal/graph/runtime/ffmpeg/FFmpegFrameView.h"
#include "internal/graph/sync/lineage/MediaVideoDisplayLineage.h"
#include "internal/graph/sync/lineage/MediaVideoReferenceLineageState.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include <array>

namespace media::ffmpeg::graph {
namespace {
::media::Status invalid(const char* message)
{ return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message)); }
}

VideoPreparationNode::VideoPreparationNode(MediaNodeId id,
    std::shared_ptr<MediaAvSyncGroupRuntime> group,
    MediaAvStartupVideoPreparationCapability preparation, MediaVideoFrameLineageProduct product)
    : FFmpegNodeRuntime(id, staticKind(), "VideoPreparationNode"), m_group(std::move(group)),
      m_preparation(std::move(preparation)), m_product(product),
      m_purge(std::make_shared<MediaOwnerThreadGenerationPurge>()),
      m_lineage(std::make_unique<MediaVideoReferenceLineageState>()) {}

::media::Status VideoPreparationNode::start(MediaGraphExecutionContext& context)
{
    if (!m_group || m_preparation.snapshot().groupKey != m_group->key() ||
        (m_product != MediaVideoFrameLineageProduct::CanonicalInterval &&
         m_product != MediaVideoFrameLineageProduct::SourceDisplayPoint))
        return invalid("Video preparation requires its source group and explicit frame product");
    if (auto status = m_purge->start(context.sharedNodeWakeup(nodeId())); !status) return status;
    auto status = FFmpegNodeRuntime::start(context);
    if (!status) clear();
    return status;
}

void VideoPreparationNode::clear() noexcept
{
    m_purge->stop();
    m_pending.reset();
    m_reservedObservation.reset();
    m_reservation.reset();
    m_generation = 0;
    m_releaseIdentity = 0;
    m_lineage->resetGenerationLifecycle();
}
::media::Status VideoPreparationNode::stop(MediaGraphExecutionContext& context)
{
    (void)m_preparation.cancel();
    auto status = FFmpegNodeRuntime::stop(context);
    clear();
    return status;
}
void VideoPreparationNode::abort(MediaGraphExecutionContext& context) noexcept
{
    (void)m_preparation.cancel();
    FFmpegNodeRuntime::abort(context);
    clear();
}

std::optional<std::uint64_t> VideoPreparationNode::generation(const MediaBufferRef& buffer) const noexcept
{
    if (m_product == MediaVideoFrameLineageProduct::SourceDisplayPoint) {
        const auto display = FFmpegFrameView::displayLineage(buffer);
        if (display && display->accessUnit) return display->accessUnit->generation;
    } else {
        if (const auto lineage = FFmpegFrameView::canonicalLineage(buffer)) return lineage->generation;
    }
    return std::nullopt;
}

bool VideoPreparationNode::pendingOutputIsCurrent(const MediaBufferRef& buffer) const noexcept
{ return m_lineage->pendingOutputIsCurrent(buffer, generation(buffer)); }

::media::Result<MediaOutputCommitReservation> VideoPreparationNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    if (!pendingOutputIsCurrent(buffer)) return Result::failure(
        ::media::ErrorInfo::cancelled("Video preparation publication is stale"));
    auto permit = m_group->reserveOutputCommit(m_generation);
    return permit ? Result::success(MediaOutputCommitReservation::hold(std::move(permit).value()))
        : Result::failure(permit.error());
}

::media::Status VideoPreparationNode::servicePurge()
{
    const auto request = m_purge->pending();
    if (!request) return ::media::Status::success();
    auto status = m_lineage->purge(*request);
    if (status) {
        cancelPendingOutputTransfer();
        m_pending.reset();
        m_reservedObservation.reset();
        m_reservation.reset();
        m_generation = request->nextGeneration;
        m_releaseIdentity = 0;
    }
    auto completed = m_purge->complete(*request, status);
    return completed ? status : completed;
}

::media::Result<MediaNodeProcessResult> VideoPreparationNode::process(MediaGraphExecutionContext& context)
{
    if (auto status = servicePurge(); !status) return processProgress(status);
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaNodeProcessResult> VideoPreparationNode::onProcess(MediaGraphExecutionContext& context)
{
    const auto preparation = m_preparation.snapshot();
    if (preparation.phase == MediaAvStartupVideoPreparationPhase::Cancelled)
        return processProgress(::media::Status::failure(
            ::media::ErrorInfo::cancelled("Video preparation was cancelled")));
    if (m_reservation) {
        if (preparation.generation != m_generation || preparation.releaseIdentity != m_releaseIdentity)
            return processProgress(invalid("Video preparation reserved release identity changed"));
        if (preparation.phase != MediaAvStartupVideoPreparationPhase::ReleaseCommitted)
            return processWaiting();
        // Acquire the source publication permit before reservation/channel locks.
        auto permit = m_group->reserveOutputCommit(m_generation);
        if (!permit) {
            // Revocation queues a purge on this owner. Keep the bounded
            // reservation until that worker transaction releases it and acks.
            if (permit.error().code == ::media::ErrorCode::Cancelled) return processWaiting();
            return processProgress(::media::Status::failure(permit.error()));
        }
        auto committed = m_reservation->commit();
        if (!committed) return processProgress(committed);
        if (m_reservedObservation) {
            MediaSourceVideoObservationContext observation;
            observation.group = m_group->key().value();
            observation.identityFromInput = true;
            observation.preparationPhase = static_cast<int>(preparation.phase);
            m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::Published, *m_reservedObservation, observation);
        }
        m_reservedObservation.reset();
        m_reservation.reset();
        m_releaseIdentity = 0;
        return processProgress();
    }
    if (preparation.phase == MediaAvStartupVideoPreparationPhase::OutputReady)
        return processProgress(invalid("Video preparation lost its reserved output"));
    bool observedInput = false;
    if (!m_pending) {
        auto input = tryPopInputOptional(context, "frame");
        if (!input) return processProgress(::media::Status::failure(input.error()));
        if (!input.value()) {
            const auto* channel = context.findInputChannel(nodeId(), "frame");
            if (channel && channel->closed()) {
                if (preparation.phase != MediaAvStartupVideoPreparationPhase::ReleaseCommitted)
                    return processProgress(invalid("Video preparation input ended before readiness"));
                return processFinished();
            }
            return processWaiting();
        }
        m_pending = std::move(*input.value());
        observedInput = true;
    }
    if (MediaRetainedControlFreshness::isCandidate(m_pending)) {
        const auto* control = dynamic_cast<const MediaControlBuffer*>(m_pending.get());
        const auto flow = control ? control->flow() : std::nullopt;
        if (!flow || (control->generation() && *control->generation() != m_generation))
            return processProgress(invalid("Video preparation control kind or generation is invalid"));
        if (preparation.phase != MediaAvStartupVideoPreparationPhase::ReleaseCommitted)
            return processProgress(invalid("Video preparation terminal precedes activation"));
        if (auto status = m_lineage->authorizeRetainedControl(m_pending); !status) return processProgress(status);
        const bool terminalFlow = *flow == MediaControlBufferFlow::Terminal;
        auto terminal = std::move(m_pending);
        auto status = emitOutput(context, "frame", terminal);
        return terminalFlow ? processFinished(status) : processProgress(status);
    }
    const auto frameGeneration = generation(m_pending);
    if (!FFmpegFrameView::frame(m_pending) || !frameGeneration)
        return processProgress(invalid("Video preparation requires its planned real frame lineage"));
    auto disposition = m_lineage->classifyObservation(*frameGeneration);
    if (!disposition) return processProgress(::media::Status::failure(disposition.error()));
    if (disposition.value() == MediaVideoLineageGenerationDisposition::DropStale) {
        m_sourceVideoObservation.droppedStale();
        m_pending.reset();
        return processProgress();
    }
    if (auto status = m_lineage->observe(*frameGeneration); !status) return processProgress(status);
    m_generation = *frameGeneration;
    MediaSourceVideoObservationContext observation;
    observation.preparationPhase = static_cast<int>(preparation.phase);
    if (observedInput)
        m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::Input, m_pending, observation);
    if (preparation.phase == MediaAvStartupVideoPreparationPhase::Feeding) {
        if (preparation.generation != m_generation || preparation.releaseIdentity == 0)
            return processProgress(invalid("Video preparation frame differs from the active feed"));
        const std::array<MediaAtomicOutputBatch, 1> batches{
            MediaAtomicOutputBatch{context.findOutputChannel(nodeId(), "frame"), std::span(&m_pending, 1)}};
        m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::OutputAttempt, m_pending, observation);
        auto reserved = MediaReservedOutputTransaction::reserve(MediaVideoPreparationReservationOwner.data(), batches, m_preparation.reservationStorage());
        if (!reserved) return processProgress(::media::Status::failure(reserved.error()));
        if (!reserved.value()) return processWaiting();
        m_reservation.emplace(std::move(*reserved.value()));
        m_reservedObservation = mediaSourceVideoObservationFacts(m_pending);
        // commit clears its buffers before returning; retain only numeric facts.
        if (m_reservedObservation) m_reservedObservation->sourceIdentity = {};
        m_releaseIdentity = preparation.releaseIdentity;
        auto ready = m_preparation.markOutputReady(m_generation, m_releaseIdentity, m_reservation->handle());
        if (!ready) {
            m_reservation.reset();
            return processProgress(ready);
        }
        m_pending.reset();
        return processWaiting();
    }
    if (preparation.phase != MediaAvStartupVideoPreparationPhase::ReleaseCommitted)
        return processProgress(invalid("Video preparation received a frame before extractor feed"));
    m_sourceVideoObservation.observe(nodeId(), name().c_str(), MediaSourceVideoObservationStage::OutputAttempt, m_pending, observation);
    auto output = std::move(m_pending);
    return processProgress(emitOutput(context, "frame", output));
}

} // namespace media::ffmpeg::graph
