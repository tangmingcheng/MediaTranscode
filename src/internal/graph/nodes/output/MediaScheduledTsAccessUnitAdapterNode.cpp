#include "internal/graph/nodes/output/MediaScheduledTsAccessUnitAdapterNode.h"

#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaTsAccessUnitBuffer.h"
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

namespace media::ffmpeg::graph {

MediaScheduledTsAccessUnitAdapterNode::MediaScheduledTsAccessUnitAdapterNode(
    MediaNodeId nodeId, MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority)
    : FFmpegNodeRuntime(nodeId, staticKind(),
                        "MediaScheduledTsAccessUnitAdapterNode"),
      m_sessionKey(std::move(sessionKey))
      , m_streamSet(streamSet)
      , m_authority(std::move(authority))
      , m_generationSession(
            std::make_shared<MediaScheduledTsAdapterGenerationState>())
      , m_generationState(
            std::make_shared<MediaProtocolOutputGenerationState>(
                std::string(generationPurgeIdentity()),
                m_generationSession))
      , m_activation(m_generationSession->activation)
      , m_transportLead(m_generationSession->transportLead)
      , m_pendingCommitGeneration(
            m_generationSession->pendingCommitGeneration)
{
}

MediaNodeKind MediaScheduledTsAccessUnitAdapterNode::staticKind() noexcept
{
    return MediaNodeKind::ScheduledTsAccessUnitAdapter;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaScheduledTsAccessUnitAdapterNode::generationPurgeTarget() const noexcept
{
    return m_generationState;
}

::media::Status MediaScheduledTsAccessUnitAdapterNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto* plan = context.findInputChannel(nodeId(), "plan");
    auto* scheduled = context.findInputChannel(nodeId(), "scheduled");
    auto* packet = context.findOutputChannel(nodeId(), "packet");
    if (!m_sessionKey.valid() || !m_authority ||
        m_authority->sessionKey() != m_sessionKey ||
        m_authority->streamSet() != m_streamSet ||
        context.inputChannels(nodeId()).size() != 2 ||
        context.outputChannels(nodeId()).size() != 1 || !plan ||
        !scheduled || !packet ||
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
    if (!m_authority) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled TS adapter requires its protocol output authority"));
    }
    std::optional<MediaProtocolOutputActivation> activation;
    std::optional<MediaRunningTime> transportLead;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        activation = m_activation;
        transportLead = m_transportLead;
    }
    if (!activation) {
        auto planInput = tryPopInputOptional(context, "plan");
        if (!planInput) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                planInput.error());
        }
        if (!planInput.value()) return processWaiting();
        const auto* plan =
            dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
                planInput.value()->get());
        if (!plan || plan->sessionKey() != m_sessionKey ||
            plan->streamSet() != m_streamSet) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Scheduled TS adapter rejects mismatched runtime plan"));
        }
        {
            auto permitted =
                m_generationState->permitActivatedGeneration(
                    *m_authority,
                    plan->activation().generation,
                    plan->activation().completedTransitionSequence);
            if (!permitted) {
                return ::media::Result<MediaNodeProcessResult>::failure(
                    permitted.error());
            }
            m_activation = plan->activation();
            m_transportLead = plan->muxPlan().transportDecodeLead();
        }
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
            {
                auto reserved = m_generationState->reserveCommit(
                    *m_authority, activation->generation);
                if (!reserved) {
                    return reserved.error().code ==
                            ::media::ErrorCode::Cancelled
                        ? processProgress()
                        : ::media::Result<MediaNodeProcessResult>::failure(
                              reserved.error());
                }
                m_pendingCommitGeneration = activation->generation;
            }
            auto forwarded = emitOutput(context, "packet", *input.value());
            return forwarded ? processProgress()
                             : processProgress(std::move(forwarded));
        }
        if (control->controlKind() == MediaControlBufferKind::Eof) {
            {
                auto reserved = m_generationState->reserveCommit(
                    *m_authority, activation->generation);
                if (!reserved) {
                    return reserved.error().code ==
                            ::media::ErrorCode::Cancelled
                        ? processProgress()
                        : ::media::Result<MediaNodeProcessResult>::failure(
                              reserved.error());
                }
                m_pendingCommitGeneration = activation->generation;
            }
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
    if (scheduled->generation() != activation->generation) {
        if (scheduled->generation() < activation->generation) {
            return processProgress();
        }
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled TS adapter rejects future generation"));
    }
    auto lead = scheduled->dispatchOnMaster().checkedSubtract(
        scheduled->emitOnMaster());
    if (!lead || !transportLead || lead.value() != *transportLead) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            lead ? ::media::ErrorInfo::invalidArgument(
                       "Scheduled TS adapter rejects transport lead mismatch")
                 : lead.error());
    }
    auto output = MediaTsAccessUnitBuffer::create(
        scheduled->media(), scheduled->stream(), scheduled->generation(),
        scheduled->presentationOnMaster(), scheduled->dispatchOnMaster(),
        scheduled->emitOnMaster(), *transportLead);
    if (!output) {
        return ::media::Result<MediaNodeProcessResult>::failure(output.error());
    }
    {
        auto reserved = m_generationState->reserveCommit(
            *m_authority, scheduled->generation());
        if (!reserved) {
            return reserved.error().code == ::media::ErrorCode::Cancelled
                ? processProgress()
                : ::media::Result<MediaNodeProcessResult>::failure(
                      reserved.error());
        }
        m_pendingCommitGeneration = scheduled->generation();
    }
    return processProgress(emitOutput(context, "packet", output.value()));
}

::media::Result<MediaOutputCommitReservation>
MediaScheduledTsAccessUnitAdapterNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    std::optional<std::uint64_t> generation;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        generation = m_pendingCommitGeneration;
    }
    if (const auto* accessUnit =
            dynamic_cast<const MediaTsAccessUnitBuffer*>(buffer.get())) {
        auto view = accessUnit->view();
        if (!view) {
            return ::media::Result<MediaOutputCommitReservation>::failure(
                        view.error());
        }
        generation = view.value().generation;
    }
    if (!generation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    ::media::ErrorInfo::notInitialized(
                        "Scheduled TS commit requires an exact generation"));
    }
    auto reservation = m_generationState->reserveCommit(
        *m_authority, *generation);
    if (!reservation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    reservation.error());
    }
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(
            std::move(reservation).value()));
}

::media::Status MediaScheduledTsAccessUnitAdapterNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    if (!m_pendingCommitGeneration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled TS output has no exact pending generation"));
    }
    if (const auto* access =
            dynamic_cast<const MediaTsAccessUnitBuffer*>(buffer.get())) {
        auto view = access->view();
        if (!view ||
            view.value().generation != *m_pendingCommitGeneration) {
            return ::media::Status::failure(
                view ? ::media::ErrorInfo::cancelled(
                           "Scheduled TS commit generation changed")
                     : view.error());
        }
    }
    if (const auto* control =
            dynamic_cast<const MediaControlBuffer*>(buffer.get());
        control && control->controlKind() == MediaControlBufferKind::Flush) {
        m_activation.reset();
        m_transportLead.reset();
    }
    m_pendingCommitGeneration.reset();
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
    m_generationState->resetLifecycle();
}

} // namespace media::ffmpeg::graph
