#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"

#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {

MediaProjectMpegTsPlanSourceNode::MediaProjectMpegTsPlanSourceNode(
    MediaNodeId nodeId, MediaAvSyncGroupKey group, MediaTsMuxPlan plan)
    : FFmpegNodeRuntime(nodeId, staticKind(),
                        "MediaProjectMpegTsPlanSourceNode"),
      m_group(std::move(group)),
      m_plan(std::move(plan)),
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
    return m_generationState;
}

::media::Status MediaProjectMpegTsPlanSourceNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto* epoch = context.findInputChannel(nodeId(), "epoch");
    auto* plan = context.findOutputChannel(nodeId(), "plan");
    m_syncGroup = context.findAvSyncGroup(m_group);
    const MediaGraph* graph = context.graph();
    const MediaNode* node = graph ? graph->findNode(nodeId()) : nullptr;
    const auto outputs = context.outputChannels(nodeId());
    bool blockingPlanOutputs = !outputs.empty();
    for (const MediaChannel* output : outputs) {
        blockingPlanOutputs = blockingPlanOutputs && output &&
            output->policy().queuePolicy.overflowPolicy ==
                MediaQueueOverflowPolicy::BlockProducer;
    }
    if (!m_group.valid() || context.inputChannels(nodeId()).size() != 1 ||
        !node || node->outputPorts.size() != 1 ||
        node->outputPorts.front().name != "plan" || !epoch || !plan ||
        !m_syncGroup ||
        !blockingPlanOutputs) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Project MPEG-TS plan source requires exact epoch and plan ports"));
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Result<MediaNodeProcessResult>
MediaProjectMpegTsPlanSourceNode::onProcess(
    MediaGraphExecutionContext& context)
{
    MediaBufferRef activationInput;
    bool published = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        published = m_published;
    }
    if (published) {
        auto duplicate = tryPopInputOptional(context, "epoch");
        if (!duplicate) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                duplicate.error());
        }
        if (!duplicate.value()) return processWaiting();
        activationInput = std::move(*duplicate.value());
        const auto* activated =
            dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
                activationInput.get());
        if (!activated || activated->groupKey() != m_group) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan source rejects duplicate activation"));
        }
        bool duplicateGeneration = false;
        {
            auto mutation = m_generationState->reserveSessionMutation();
            duplicateGeneration =
                m_publishedGeneration &&
                *m_publishedGeneration == activated->epoch().generation;
            if (!duplicateGeneration) m_published = false;
        }
        if (duplicateGeneration) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan source rejects duplicate activation"));
        }
    }
    bool hasPendingPlan = false;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        hasPendingPlan = static_cast<bool>(m_pendingPlan);
    }
    if (!hasPendingPlan) {
        if (!activationInput) {
            auto input = tryPopInputOptional(context, "epoch");
            if (!input) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    input.error());
            }
            if (!input.value()) return processWaiting();
            activationInput = std::move(*input.value());
        }
        const auto* activated =
            dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
                activationInput.get());
        if (!activated || activated->groupKey() != m_group) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan source rejects mismatched activation"));
        }
        auto created = MediaTsMuxRuntimePlanBuffer::create(
            m_plan, activated->epoch(), m_group,
            activated->completedTransitionSequence());
        if (!created) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                created.error());
        }
        {
            auto permitted =
                m_generationState->permitActivatedGeneration(
                    *m_syncGroup,
                    activated->epoch().generation,
                    activated->completedTransitionSequence());
            if (!permitted) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    permitted.error());
            }
            m_pendingPlan = std::move(created).value();
            m_publishedGeneration = activated->epoch().generation;
        }
    }
    MediaBufferRef pendingPlan;
    std::optional<std::uint64_t> publishedGeneration;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        pendingPlan = m_pendingPlan;
        publishedGeneration = m_publishedGeneration;
    }
    if (!pendingPlan || !publishedGeneration) {
        return processWaiting();
    }
    auto committed = emitOutput(context, "plan", pendingPlan);
    return committed ? processProgress()
                     : processProgress(std::move(committed));
}

::media::Result<MediaOutputCommitReservation>
MediaProjectMpegTsPlanSourceNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    const auto* plan =
        dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(buffer.get());
    if (!plan || !m_syncGroup) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Project MPEG-TS plan commit requires a runtime plan and sync group"));
    }
    auto reservation =
        m_generationState->reserveCommit(
            *m_syncGroup, plan->epoch().generation);
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
        dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(buffer.get());
    if (!plan || !m_pendingPlan || !m_publishedGeneration ||
        plan->epoch().generation != *m_publishedGeneration) {
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
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaProjectMpegTsPlanSourceNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaProjectMpegTsPlanSourceNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_syncGroup.reset();
    m_generationState->resetLifecycle();
}

} // namespace media::ffmpeg::graph
