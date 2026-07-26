#include "internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"

namespace media::ffmpeg::graph {

MediaScheduledTsAccessUnitAdapterNode::MediaScheduledTsAccessUnitAdapterNode(
    MediaNodeId nodeId, MediaAvSyncGroupKey group)
    : FFmpegNodeRuntime(nodeId, staticKind(),
                        "MediaScheduledTsAccessUnitAdapterNode"),
      m_group(std::move(group))
{
}

MediaNodeKind MediaScheduledTsAccessUnitAdapterNode::staticKind() noexcept
{
    return MediaNodeKind::ScheduledTsAccessUnitAdapter;
}

::media::Status MediaScheduledTsAccessUnitAdapterNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto* plan = context.findInputChannel(nodeId(), "plan");
    auto* scheduled = context.findInputChannel(nodeId(), "scheduled");
    auto* packet = context.findOutputChannel(nodeId(), "packet");
    m_syncGroup = context.findAvSyncGroup(m_group);
    if (!m_group.valid() || context.inputChannels(nodeId()).size() != 2 ||
        context.outputChannels(nodeId()).size() != 1 || !plan ||
        !scheduled || !packet || !m_syncGroup ||
        packet->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Scheduled TS adapter requires exact plan, scheduled, and packet ports"));
    }
    return FFmpegNodeRuntime::start(context);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledTsAccessUnitAdapterNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (!m_syncGroup) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled TS adapter requires its registered sync group"));
    }
    const auto snapshot = m_syncGroup->epochTransitionSnapshot();
    if (snapshot.poisoned) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::cancelled(
                "Scheduled TS adapter output authority is poisoned"));
    }
    if (!snapshot.outputPermitted || !snapshot.playbackEpoch) {
        cancelPendingOutputTransfer();
        return processWaiting();
    }
    if (m_epoch &&
        m_epoch->generation != snapshot.playbackEpoch->generation) {
        cancelPendingOutputTransfer();
        m_epoch.reset();
        m_transportLead.reset();
    }
    if (!m_epoch) {
        auto planInput = tryPopInputOptional(context, "plan");
        if (!planInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                planInput.error());
        }
        if (!planInput.value()) return processWaiting();
        const auto* plan = dynamic_cast<const MediaTsMuxRuntimePlanBuffer*>(
            planInput.value()->get());
        if (!plan || plan->group() != m_group ||
            plan->epoch() != *snapshot.playbackEpoch) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Scheduled TS adapter rejects mismatched runtime plan"));
        }
        m_epoch = plan->epoch();
        m_transportLead = plan->plan().transportDecodeLead();
        return processProgress();
    }
    auto input = tryPopInputOptional(context, "scheduled");
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) return processWaiting();
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            input.value()->get())) {
        if (control->controlKind() == MediaControlBufferKind::Flush) {
            auto forwarded = emitOutput(context, "packet", *input.value());
            if (forwarded ||
                forwarded.error().code == ::media::ErrorCode::WouldBlock) {
                m_epoch.reset();
                m_transportLead.reset();
            }
            return forwarded ? processProgress()
                             : processProgress(std::move(forwarded));
        }
        if (control->controlKind() == MediaControlBufferKind::Eof) {
            return processFinished(
                emitOutput(context, "packet", *input.value()));
        }
        return ::media::Result<MediaNodeProcessResult>::failure(
            control->controlKind() == MediaControlBufferKind::Abort
                ? ::media::ErrorInfo::cancelled(
                      "Scheduled TS adapter received abort")
                : ::media::ErrorInfo::invalidArgument(
                      "Scheduled TS adapter rejects unknown control"));
    }
    const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(
        input.value()->get());
    if (!scheduled) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled TS adapter rejects generation mismatch"));
    }
    if (scheduled->generation() != m_epoch->generation) {
        if (scheduled->generation() < m_epoch->generation) {
            return processProgress();
        }
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled TS adapter rejects future generation"));
    }
    if (auto permitted = validateOutputPermit(scheduled->generation());
        !permitted) {
        return processWaiting();
    }
    auto lead = scheduled->dispatchOnMaster().checkedSubtract(
        scheduled->emitOnMaster());
    if (!lead || lead.value() != *m_transportLead) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            lead ? ::media::ErrorInfo::invalidArgument(
                       "Scheduled TS adapter rejects transport lead mismatch")
                 : lead.error());
    }
    auto output = MediaTsAccessUnitBuffer::create(
        scheduled->media(), scheduled->stream(), scheduled->generation(),
        scheduled->presentationOnMaster(), scheduled->dispatchOnMaster(),
        scheduled->emitOnMaster(), *m_transportLead);
    if (!output) {
        return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    }
    return processProgress(emitOutput(context, "packet", output.value()));
}

::media::Status
MediaScheduledTsAccessUnitAdapterNode::validateOutputPermit(
    std::uint64_t generation) const
{
    if (!m_syncGroup) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled TS adapter has no output authority"));
    }
    const auto snapshot = m_syncGroup->epochTransitionSnapshot();
    if (snapshot.poisoned || !snapshot.outputPermitted ||
        !snapshot.playbackEpoch ||
        snapshot.playbackEpoch->generation != generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled(
                "Scheduled TS output permit is closed for this generation"));
    }
    return ::media::Status::success();
}

::media::Status MediaScheduledTsAccessUnitAdapterNode::flush(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::flush(context);
}

::media::Status MediaScheduledTsAccessUnitAdapterNode::stop(
    MediaGraphExecutionContext& context)
{
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaScheduledTsAccessUnitAdapterNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void MediaScheduledTsAccessUnitAdapterNode::resetState() noexcept
{
    cancelPendingOutputTransfer();
    m_syncGroup.reset();
    m_epoch.reset();
    m_transportLead.reset();
}

} // namespace media::ffmpeg::graph
