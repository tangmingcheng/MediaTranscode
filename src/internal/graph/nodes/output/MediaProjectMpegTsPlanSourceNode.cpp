#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"

#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/runtime/buffer/MediaDatagramTransportPlanBuffer.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/protocol/MediaProtocolPurgeInput.h"
#include <array>

namespace media::ffmpeg::graph {

MediaProjectMpegTsPlanSourceNode::MediaProjectMpegTsPlanSourceNode(
    MediaNodeId nodeId, MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaProjectMpegTsRuntimeOutputPlan outputPlan,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(nodeId, staticKind(),
                        "MediaProjectMpegTsPlanSourceNode"),
      m_sessionKey(std::move(sessionKey)),
      m_streamSet(streamSet),
      m_outputPlan(
          std::make_shared<const MediaProjectMpegTsRuntimeOutputPlan>(
              std::move(outputPlan))),
      m_authority(std::move(authority)),
      m_generationSession(
          std::make_shared<
              MediaProjectMpegTsPlanSourceGenerationState>()),
      m_generationState(
          std::make_shared<MediaProtocolOutputGenerationState>(
              std::string(generationPurgeIdentity()),
              m_generationSession)),
      m_pendingPlan(m_generationSession->pendingPlan),
      m_publishedGeneration(
          m_generationSession->publishedGeneration),
      m_published(m_generationSession->published)
{
}

MediaNodeKind MediaProjectMpegTsPlanSourceNode::staticKind() noexcept
{
    return MediaNodeKind::ProjectMpegTsPlanSource;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaProjectMpegTsPlanSourceNode::generationPurgeTarget() const noexcept
{
    return m_generationPurge;
}

::media::Status MediaProjectMpegTsPlanSourceNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    m_completedPurge.reset();
    auto started = m_generationPurge->start(context.sharedNodeWakeup(nodeId()));
    if (!started) return started;
    auto* activation = context.findInputChannel(nodeId(), "activation");
    auto* plan = context.findOutputChannel(nodeId(), "plan");
    auto* transport = context.findInputChannel(nodeId(), "transport_plan");
    const MediaGraph* graph = context.graph();
    const MediaNode* node = graph ? graph->findNode(nodeId()) : nullptr;
    const auto outputs = context.outputChannels(nodeId());
    bool blockingPlanOutputs = !outputs.empty();
    for (const MediaChannel* output : outputs) {
        blockingPlanOutputs = blockingPlanOutputs && output &&
            output->policy().queuePolicy.overflowPolicy ==
                MediaQueueOverflowPolicy::BlockProducer;
    }
    if (!m_sessionKey.valid() || !m_authority ||
        m_authority->sessionKey() != m_sessionKey ||
        m_authority->streamSet() != m_streamSet ||
        context.inputChannels(nodeId()).size() != 2 ||
        !node || node->outputPorts.size() != 1 ||
        node->outputPorts.front().name != "plan" || !activation || !plan || !transport ||
        transport->binding().payloadKind != MediaPayloadKind::DatagramTransportPlan ||
        !blockingPlanOutputs) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS plan source requires exact activation, transport and plan ports"));
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Result<MediaNodeProcessResult>
MediaProjectMpegTsPlanSourceNode::process(MediaGraphExecutionContext& context)
{
    if (auto purge = m_generationPurge->pending()) {
        auto activation = m_authority->currentActivation();
        if (!activation || activation.value().generation != purge->publishedGeneration ||
            purge->oldGeneration < purge->publishedGeneration) {
            auto status = ::media::Status::failure(activation
                ? ::media::ErrorInfo::invalidArgument("MPEG-TS plan purge differs from revoked authority")
                : activation.error());
            auto completed = m_generationPurge->complete(*purge, status);
            return processProgress(completed ? status : completed);
        }
        auto status = ::media::Status::success();
        if (m_generationSession->transportPlan)
            status = authorizeProtocolPurgeInput(m_generationSession->transportPlan,
                m_sessionKey, *purge, m_completedPurge);
        for (const auto* port : std::array{"activation", "transport_plan"}) {
            while (status) {
                auto input = tryPopInputOptional(context, port);
                if (!input) { status = ::media::Status::failure(input.error()); break; }
                if (!input.value()) break;
                status = authorizeProtocolPurgeInput(*input.value(), m_sessionKey, *purge, m_completedPurge);
            }
            if (!status) break;
        }
        if (status) {
            cancelPendingOutputTransfer();
            status = m_generationState->purge(*purge);
        }
        if (status) m_completedPurge = *purge;
        auto completed = m_generationPurge->complete(*purge, status);
        return processProgress(completed ? status : completed);
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaNodeProcessResult>
MediaProjectMpegTsPlanSourceNode::onProcess(
    MediaGraphExecutionContext& context)
{
    using Result = ::media::Result<MediaNodeProcessResult>;
    MediaBufferRef pending;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        pending = m_pendingPlan;
    }
    if (pending) return processProgress(emitOutput(context, "plan", pending));

    auto activationInput = tryPopInputOptional(context, "activation");
    if (!activationInput) return Result::failure(activationInput.error());
    auto transportInput = tryPopInputOptional(context, "transport_plan");
    if (!transportInput) return Result::failure(transportInput.error());
    std::optional<MediaProtocolOutputActivation> activation;
    if (activationInput.value()) {
        auto validated = m_authority->validateActivation(*activationInput.value());
        if (!validated) return Result::failure(validated.error());
        activation = validated.value();
    }
    if (transportInput.value()) {
        const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(
            transportInput.value()->get());
        if (!transport || transport->plan().shaping.sessionKey() != m_sessionKey.value() ||
            !transport->globalSequence() ||
            transport->globalSequence()->sessionKey() != m_sessionKey.value() ||
            transport->globalSequence()->generation() != transport->plan().shaping.generation())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires its exact typed transport session"));
    }
    MediaBufferRef transportBuffer;
    bool published = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if ((activation && m_generationSession->activation) ||
            (transportInput.value() && m_generationSession->transportPlan))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan rejects duplicate generation bindings"));
        if (activation) m_generationSession->activation = activation;
        if (transportInput.value()) m_generationSession->transportPlan = *transportInput.value();
        activation = m_generationSession->activation;
        transportBuffer = m_generationSession->transportPlan;
        published = m_published;
    }
    const auto* activationChannel = context.findInputChannel(nodeId(), "activation");
    const auto* transportChannel = context.findInputChannel(nodeId(), "transport_plan");
    if (published) {
        return activationChannel->closed() && transportChannel->closed()
            ? processFinished() : processWaiting();
    }
    if (!activation || !transportBuffer) {
        if ((!activation && activationChannel->closed()) ||
            (!transportBuffer && transportChannel->closed()))
            return Result::failure(::media::ErrorInfo::notInitialized(
                "MPEG-TS runtime plan input closed before its required binding"));
        return processWaiting();
    }
    const auto* transport = dynamic_cast<const MediaDatagramTransportPlanBuffer*>(transportBuffer.get());
    auto created = MediaProjectMpegTsRuntimePlanBuffer::create(
        m_outputPlan, m_sessionKey, m_streamSet, *activation, *transport);
    if (!created) return Result::failure(created.error());
    {
        auto permitted = m_generationState->permitActivatedGeneration(
            *m_authority, activation->generation, activation->completedTransitionSequence);
        if (!permitted) return Result::failure(
            permitted.error().code == ::media::ErrorCode::Cancelled
                ? ::media::ErrorInfo::wouldBlock("MPEG-TS runtime plan activation awaits generation purge")
                : permitted.error());
        m_pendingPlan = std::move(created).value();
        m_publishedGeneration = activation->generation;
        pending = m_pendingPlan;
    }
    return processProgress(emitOutput(context, "plan", pending));
}

::media::Result<MediaOutputCommitReservation>
MediaProjectMpegTsPlanSourceNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    const auto* plan =
        dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
            buffer.get());
    if (!plan || !m_authority) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    ::media::ErrorInfo::notInitialized(
            "Project MPEG-TS plan commit requires a runtime plan and sync group"));
    }
    auto reservation =
        m_generationState->reserveCommit(
            *m_authority, plan->activation().generation);
    if (!reservation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    reservation.error());
    }
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(
            std::move(reservation).value()));
}

::media::Status MediaProjectMpegTsPlanSourceNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    const auto* plan =
        dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
            buffer.get());
    if (!plan || !m_pendingPlan || !m_publishedGeneration ||
        plan->activation().generation != *m_publishedGeneration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled(
                "Project MPEG-TS plan commit differs from its exact generation state"));
    }
    m_pendingPlan.reset();
    m_published = true;
    return ::media::Status::success();
}

::media::Status MediaProjectMpegTsPlanSourceNode::stop(
    MediaGraphExecutionContext& context)
{
    m_generationPurge->stop();
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaProjectMpegTsPlanSourceNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_generationPurge->stop();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaProjectMpegTsPlanSourceNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_generationState->resetLifecycle();
}

} // namespace media::ffmpeg::graph
