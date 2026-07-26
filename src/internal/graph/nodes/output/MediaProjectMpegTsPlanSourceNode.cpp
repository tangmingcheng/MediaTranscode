#include "internal/graph/nodes/output/MediaProjectMpegTsPlanSourceNode.h"

#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

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
    if (m_published) {
        auto duplicate = tryPopInputOptional(context, "epoch");
        if (!duplicate) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                duplicate.error());
        }
        if (duplicate.value()) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan source rejects duplicate activation"));
        }
        return processFinished();
    }
    if (!m_pendingPlan) {
        auto input = tryPopInputOptional(context, "epoch");
        if (!input) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                input.error());
        }
        if (!input.value()) return processWaiting();
        const auto* activated =
            dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
                input.value()->get());
        if (!activated || activated->groupKey() != m_group) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Project MPEG-TS plan source rejects mismatched activation"));
        }
        if (auto observed = m_generationState->observe(
                activated->epoch().generation); !observed) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                observed.error());
        }
        auto created = MediaTsMuxRuntimePlanBuffer::create(
            m_plan, activated->epoch(), m_group);
        if (!created) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                created.error());
        }
        m_pendingPlan = std::move(created).value();
    }
    auto committed = emitOutput(context, "plan", m_pendingPlan);
    m_pendingPlan.reset();
    m_published = true;
    return committed ? processProgress()
                     : processProgress(std::move(committed));
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
    m_published = false;
    m_generationState->resetLifecycle();
}

} // namespace media::ffmpeg::graph
