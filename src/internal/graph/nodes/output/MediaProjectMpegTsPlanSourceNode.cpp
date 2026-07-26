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
      m_generationState(
          std::make_shared<MediaProtocolOutputGenerationState>(
              std::string(generationPurgeIdentity())))
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
    if (m_published) {
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
        if (!activated || activated->groupKey() != m_group ||
            (m_publishedGeneration &&
             *m_publishedGeneration == activated->epoch().generation)) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan source rejects duplicate activation"));
        }
        m_published = false;
    }
    if (!m_pendingPlan) {
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
        const auto snapshot = m_syncGroup->epochTransitionSnapshot();
        if (snapshot.poisoned || !snapshot.outputPermitted ||
            !snapshot.playbackEpoch ||
            *snapshot.playbackEpoch != activated->epoch()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::cancelled(
                    "Project MPEG-TS plan source output permit is closed"));
        }
        if (auto permitted =
                m_generationState->permitActivatedGeneration(
                    activated->epoch().generation);
            !permitted) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                permitted.error());
        }
        auto created = MediaTsMuxRuntimePlanBuffer::create(
            m_plan, activated->epoch(), m_group);
        if (!created) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                created.error());
        }
        m_pendingPlan = std::move(created).value();
        m_publishedGeneration = activated->epoch().generation;
    }
    const auto snapshot = m_syncGroup->epochTransitionSnapshot();
    if (!m_publishedGeneration || snapshot.poisoned ||
        !snapshot.outputPermitted || !snapshot.playbackEpoch ||
        snapshot.playbackEpoch->generation != *m_publishedGeneration) {
        m_pendingPlan.reset();
        return processWaiting();
    }
    auto committed = emitOutput(context, "plan", m_pendingPlan);
    m_pendingPlan.reset();
    m_published = true;
    return committed ? processProgress()
                     : processProgress(std::move(committed));
}

::media::Result<
    std::optional<MediaProtocolOutputGenerationCommitReservation>>
MediaProjectMpegTsPlanSourceNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    const auto* plan =
        dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(buffer.get());
    if (!plan || !m_syncGroup) {
        return ::media::Result<
            std::optional<
                MediaProtocolOutputGenerationCommitReservation>>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Project MPEG-TS plan commit requires a runtime plan and sync group"));
    }
    const auto snapshot = m_syncGroup->epochTransitionSnapshot();
    if (snapshot.poisoned || !snapshot.outputPermitted ||
        !snapshot.playbackEpoch ||
        snapshot.playbackEpoch->generation != plan->epoch().generation) {
        return ::media::Result<
            std::optional<
                MediaProtocolOutputGenerationCommitReservation>>::failure(
                    ::media::ErrorInfo::cancelled(
                        "Project MPEG-TS plan commit is closed for this generation"));
    }
    auto reservation =
        m_generationState->reserveCommit(plan->epoch().generation);
    if (!reservation) {
        return ::media::Result<
            std::optional<
                MediaProtocolOutputGenerationCommitReservation>>::failure(
                    reservation.error());
    }
    return ::media::Result<
        std::optional<MediaProtocolOutputGenerationCommitReservation>>::
        success(std::optional<MediaProtocolOutputGenerationCommitReservation>(
            std::move(reservation).value()));
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
    m_pendingPlan.reset();
    m_publishedGeneration.reset();
    m_published = false;
    m_syncGroup.reset();
    m_generationState->resetLifecycle();
}

} // namespace media::ffmpeg::graph
