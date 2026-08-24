#include "internal/graph/nodes/output/MediaDatagramTransportPlanSourceNode.h"

#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {

MediaDatagramTransportPlanSourceNode::MediaDatagramTransportPlanSourceNode(
    MediaNodeId nodeId,
    MediaDatagramTransportPlanTemplate planTemplate,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(nodeId, staticKind(),
                        "MediaDatagramTransportPlanSourceNode"),
      m_planTemplate(std::move(planTemplate)),
      m_authority(std::move(authority)),
      m_generationSession(
          std::make_shared<MediaDatagramTransportPlanSourceGenerationState>()),
      m_generationState(std::make_shared<MediaProtocolOutputGenerationState>(
          std::string(generationPurgeIdentity()), m_generationSession)),
      m_pendingPlan(m_generationSession->pendingPlan),
      m_pendingGeneration(m_generationSession->pendingGeneration),
      m_publishedGeneration(m_generationSession->publishedGeneration)
{
}

::media::Result<std::unique_ptr<MediaDatagramTransportPlanSourceNode>>
MediaDatagramTransportPlanSourceNode::create(
    MediaNodeId nodeId,
    MediaDatagramTransportPlanTemplate planTemplate,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
{
    using Result = ::media::Result<
        std::unique_ptr<MediaDatagramTransportPlanSourceNode>>;
    if (!nodeId.isValid() || !authority ||
        planTemplate.sessionKey() != authority->sessionKey().value() ||
        planTemplate.serviceScopeId().empty() ||
        planTemplate.remoteEndpoints().empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport plan source requires matching session, scope, endpoints, and runtime authority"));
    }
    try {
        return Result::success(
            std::unique_ptr<MediaDatagramTransportPlanSourceNode>(
                new MediaDatagramTransportPlanSourceNode(
                    nodeId, std::move(planTemplate), std::move(authority))));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MediaDatagramTransportPlanSourceNode"));
    }
}

MediaNodeKind MediaDatagramTransportPlanSourceNode::staticKind() noexcept
{
    return MediaNodeKind::DatagramTransportPlanSource;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaDatagramTransportPlanSourceNode::generationPurgeTarget() const noexcept
{
    return m_generationState;
}

::media::Status MediaDatagramTransportPlanSourceNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    const auto* activation = context.findInputChannel(nodeId(), "activation");
    const auto* plan = context.findOutputChannel(nodeId(), "plan");
    if (context.inputChannels(nodeId()).size() != 1 ||
        context.outputChannels(nodeId()).empty() || !activation || !plan ||
        plan->binding().streamKind != MediaStreamKind::Metadata ||
        plan->binding().payloadKind != MediaPayloadKind::DatagramTransportPlan) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Datagram transport plan source requires exact activation and typed plan ports"));
    }
    for (const auto* output : context.outputChannels(nodeId())) {
        if (!output || output->policy().queuePolicy.overflowPolicy !=
                           MediaQueueOverflowPolicy::BlockProducer) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Datagram transport plan fanout requires blocking outputs"));
        }
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Result<MediaNodeProcessResult>
MediaDatagramTransportPlanSourceNode::onProcess(
    MediaGraphExecutionContext& context)
{
    MediaBufferRef pendingPlan;
    std::optional<std::uint64_t> publishedGeneration;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        pendingPlan = m_pendingPlan;
        publishedGeneration = m_publishedGeneration;
    }
    if (pendingPlan) {
        auto emitted = emitOutput(context, "plan", pendingPlan);
        return emitted ? processProgress()
                       : processProgress(std::move(emitted));
    }
    auto input = tryPopInputOptional(context, "activation");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        const auto* channel = context.findInputChannel(nodeId(), "activation");
        return channel && channel->closed() ? processFinished()
                                            : processWaiting();
    }
    auto activation = m_authority->validateActivation(*input.value());
    if (!activation) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            activation.error());
    }
    if (publishedGeneration &&
        activation.value().generation <= *publishedGeneration) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Datagram transport plan source rejects duplicate or regressed generation"));
    }
    auto permitted = m_generationState->permitActivatedGeneration(
        *m_authority, activation.value().generation,
        activation.value().completedTransitionSequence);
    if (!permitted) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            permitted.error());
    }
    auto created = MediaDatagramTransportPlanBuffer::create(
        m_planTemplate, activation.value().generation);
    if (!created) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            created.error());
    }
    {
        auto mutation = m_generationState->reserveSessionMutation();
        m_pendingPlan = std::move(created).value();
        m_pendingGeneration = activation.value().generation;
        pendingPlan = m_pendingPlan;
    }
    auto emitted = emitOutput(context, "plan", pendingPlan);
    return emitted ? processProgress() : processProgress(std::move(emitted));
}

::media::Result<MediaOutputCommitReservation>
MediaDatagramTransportPlanSourceNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    const auto* plan = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
        buffer.get());
    if (!plan) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
            ::media::ErrorInfo::cancelled(
                "Datagram transport plan commit generation changed"));
    }
    auto reservation = m_generationState->reserveCommit(
        *m_authority, plan->plan().shaping.generation());
    if (!reservation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
            reservation.error());
    }
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(std::move(reservation).value()));
}

::media::Status MediaDatagramTransportPlanSourceNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    const auto* plan = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
        buffer.get());
    auto mutation = m_generationState->reserveSessionMutation();
    if (!plan || !m_pendingGeneration || !m_pendingPlan ||
        plan->plan().shaping.generation() != *m_pendingGeneration ||
        buffer != m_pendingPlan) {
        return ::media::Status::failure(::media::ErrorInfo::cancelled(
            "Datagram transport plan commit differs from pending generation"));
    }
    m_publishedGeneration = *m_pendingGeneration;
    m_pendingGeneration.reset();
    m_pendingPlan.reset();
    return ::media::Status::success();
}

::media::Status MediaDatagramTransportPlanSourceNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaDatagramTransportPlanSourceNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaDatagramTransportPlanSourceNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_generationState->resetLifecycle();
}

} // namespace media::ffmpeg::graph
