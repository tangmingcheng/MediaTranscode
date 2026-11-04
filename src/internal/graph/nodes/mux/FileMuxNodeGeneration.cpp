#include "internal/graph/nodes/mux/FileMuxNode.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/protocol/MediaProtocolPurgeInput.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include <array>

namespace media::ffmpeg::graph {
::media::Status FileMuxNode::start(MediaGraphExecutionContext& context)
{
    if (m_generationPurge) {
        if (!m_generationState || !m_outputAuthority)
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "Realtime mux requires its generation state and output authority"));
        m_completedPurge.reset();
        auto reset = m_generationState->resetLifecycle();
        if (!reset) return reset;
        auto started = m_generationPurge->start(context.sharedNodeWakeup(nodeId()));
        if (!started) return started;
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Status FileMuxNode::applyGenerationPurge(MediaGraphExecutionContext& context,
    const MediaAvGenerationPurge& purge)
{
    auto activation = m_outputAuthority->currentActivation();
    if (!activation || activation.value().generation != purge.publishedGeneration ||
        purge.oldGeneration < purge.publishedGeneration)
        return ::media::Status::failure(activation ? ::media::ErrorInfo::invalidArgument(
            "Mux purge differs from its revoked generation") : activation.error());
    // Run only on the owner thread, after the prior process call and all its
    // publication permits/local output references have left scope.
    if (m_pendingSessionInput) {
        auto authorized = authorizeProtocolPurgeInput(m_pendingSessionInput, m_outputAuthority->sessionKey(),
            purge, m_completedPurge);
        if (!authorized) return authorized;
    }
    cancelPendingOutputTransfer();
    m_pendingSessionInput.reset();
    m_pendingFlushForward = false;
    static constexpr std::array<const char*, 2> generationInputs{"plan", "packet"};
    for (const auto* port : generationInputs) {
        while (true) {
            auto input = tryPopInputOptional(context, port);
            if (!input) return ::media::Status::failure(input.error());
            if (!input.value()) break;
            auto authorized = authorizeProtocolPurgeInput(*input.value(), m_outputAuthority->sessionKey(),
                purge, m_completedPurge);
            if (!authorized) return authorized;
        }
    }
    // Codec bindings have no generation and remain valid across source reentry.
    // Do not acquire an output permit while holding the session/state locks.
    auto status = m_generationState->purge(purge);
    if (status) {
        if (m_completion) m_completion->reset();
        for (auto& [id, binding] : m_bindingInputs) {
            if (binding.portName == "plan") binding.satisfied = false;
        }
        m_phase = Phase::AcquiringBindings;
        m_completedPurge = purge;
    }
    return status;
}

::media::Result<MediaNodeProcessResult> FileMuxNode::process(MediaGraphExecutionContext& context)
{
    if (m_generationPurge) {
        if (const auto purge = m_generationPurge->pending()) {
            auto result = applyGenerationPurge(context, *purge);
            auto completed = m_generationPurge->complete(*purge, result);
            if (!completed) return processProgress(remember(completed));
            return processProgress(remember(result));
        }
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaOutputCommitReservation> FileMuxNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    using Result = ::media::Result<MediaOutputCommitReservation>;
    if (!m_generationPurge) return FFmpegNodeRuntime::reserveOutputCommit(buffer);
    std::optional<std::uint64_t> generation;
    if (const auto* batch = dynamic_cast<const MediaMpegTsProtocolDatagramBatchBuffer*>(buffer.get())) {
        generation = batch->generation();
    } else if (const auto* control = dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
        generation = control->generation();
        if (!generation && m_outputAuthority->streamSet() == MediaTranscodeStreamSet::VideoOnly) {
            auto activation = m_outputAuthority->currentActivation();
            if (!activation) return Result::failure(activation.error());
            generation = activation.value().generation;
        }
    }
    if (!generation || *generation == 0) return Result::failure(::media::ErrorInfo::invalidArgument(
        "Realtime mux publication requires exact protocol generation"));
    auto reserved = m_generationState->reserveCommit(*m_outputAuthority, *generation);
    if (!reserved) return Result::failure(reserved.error());
    return Result::success(MediaOutputCommitReservation::hold(std::move(reserved).value()));
}
} // namespace media::ffmpeg::graph
