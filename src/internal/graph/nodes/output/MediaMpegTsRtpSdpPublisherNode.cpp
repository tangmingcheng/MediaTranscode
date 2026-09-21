#include "internal/graph/nodes/output/MediaMpegTsRtpSdpPublisherNode.h"

#include "internal/graph/protocol/sdp/MediaMpegTsRtpSdpDescription.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"

#include <new>
#include <utility>
#include <variant>

namespace media::ffmpeg::graph {

MediaMpegTsRtpSdpPublisherNode::MediaMpegTsRtpSdpPublisherNode(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSession,
    MediaTranscodeStreamSet streamSet,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
    std::unique_ptr<MediaAtomicFileReplacePort> replacePort)
    : FFmpegNodeRuntime(
          nodeId, staticKind(), "MediaMpegTsRtpSdpPublisherNode"),
      m_plannedSession(std::move(plannedSession)),
      m_streamSet(streamSet),
      m_authority(std::move(authority)),
      m_replacePort(std::move(replacePort))
{
}

::media::Result<std::unique_ptr<MediaMpegTsRtpSdpPublisherNode>>
MediaMpegTsRtpSdpPublisherNode::create(
    MediaNodeId nodeId,
    MediaProtocolOutputSessionKey plannedSession,
    MediaTranscodeStreamSet streamSet,
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> authority,
    std::unique_ptr<MediaAtomicFileReplacePort> replacePort)
{
    using NodeResult = ::media::Result<
        std::unique_ptr<MediaMpegTsRtpSdpPublisherNode>>;
    if (!nodeId.isValid() || !plannedSession.valid() || !authority ||
        authority->sessionKey() != plannedSession ||
        authority->streamSet() != streamSet || !replacePort) {
        return NodeResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T SDP publisher requires an exact output authority and atomic replace port"));
    }
    auto node = std::unique_ptr<MediaMpegTsRtpSdpPublisherNode>(
        new (std::nothrow) MediaMpegTsRtpSdpPublisherNode(
            nodeId, std::move(plannedSession), streamSet,
            std::move(authority),
            std::move(replacePort)));
    if (!node) {
        return NodeResult::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaMpegTsRtpSdpPublisherNode"));
    }
    try { node->m_generationPurge = std::make_shared<MediaOwnerThreadGenerationPurge>(); }
    catch (const std::bad_alloc&) { return NodeResult::failure(::media::ErrorInfo::allocationFailed("MP2T SDP purge")); }
    return NodeResult::success(std::move(node));
}

MediaNodeKind MediaMpegTsRtpSdpPublisherNode::staticKind() noexcept
{
    return MediaNodeKind::MpegTsRtpSdpPublisher;
}

::media::Status MediaMpegTsRtpSdpPublisherNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const MediaChannel* plan = context.findInputChannel(
        nodeId(), "plan");
    if (context.inputChannels(nodeId()).size() != 1 ||
        !context.outputChannels(nodeId()).empty() || !plan ||
        plan->binding().streamKind != MediaStreamKind::Metadata) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "MP2T SDP publisher requires exactly one metadata plan input"));
    }
    return ::media::Status::success();
}

::media::Status MediaMpegTsRtpSdpPublisherNode::start(
    MediaGraphExecutionContext& context)
{
    resetState();
    auto valid = validatePorts(context);
    if (!valid) return valid;
    m_completedPurge.reset();
    auto started = m_generationPurge->start(context.sharedNodeWakeup(nodeId()));
    return started ? FFmpegNodeRuntime::start(context) : started;
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsRtpSdpPublisherNode::failTerminal(
    ::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    return ::media::Result<MediaNodeProcessResult>::failure(
        *m_terminalFailure);
}

::media::Status MediaMpegTsRtpSdpPublisherNode::applyGenerationPurge(
    MediaGraphExecutionContext& context, const MediaAvGenerationPurge& purge)
{
    const auto authorized = [&](std::uint64_t generation) {
        return generation == purge.oldGeneration ||
            (m_completedPurge && generation == m_completedPurge->oldGeneration);
    };
    const auto discard = [&](const MediaBufferRef& buffer) -> ::media::Status {
        if (const auto* control = dynamic_cast<const MediaControlBuffer*>(buffer.get())) {
            if (control->controlKind() == MediaControlBufferKind::Abort)
                return ::media::Status::failure(::media::ErrorInfo::cancelled("SDP publisher aborted during purge"));
            if (!control->generation() || !authorized(*control->generation()) ||
                (control->controlKind() != MediaControlBufferKind::Eof &&
                 control->controlKind() != MediaControlBufferKind::Flush))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("SDP control is outside exact purge authorization"));
            return ::media::Status::success();
        }
        const auto* plan = dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(buffer.get());
        if (!plan || !authorized(plan->activation().generation) ||
            plan->sessionKey() != m_plannedSession || plan->streamSet() != m_streamSet)
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("MP2T SDP plan is outside exact purge authorization"));
        return ::media::Status::success();
    };
    if (m_pendingPlan) {
        auto status = discard(m_pendingPlan);
        if (!status) return status;
        m_pendingPlan.reset();
    }
    for (auto* channel : context.inputChannels(nodeId())) {
        if (!channel || channel->aborted())
            return ::media::Status::failure(::media::ErrorInfo::cancelled("SDP publisher input aborted during purge"));
        MediaBufferRef buffer;
        while (channel->tryPop(buffer)) {
            auto status = discard(buffer);
            if (!status) return status;
        }
    }
    m_completedPurge = purge;
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult> MediaMpegTsRtpSdpPublisherNode::process(
    MediaGraphExecutionContext& context)
{
    if (const auto purge = m_generationPurge->pending()) {
        auto status = applyGenerationPurge(context, *purge);
        auto completed = m_generationPurge->complete(*purge, status);
        if (!completed) return failTerminal(completed.error());
        if (!status) return failTerminal(status.error());
        return processProgress();
    }
    return FFmpegNodeRuntime::process(context);
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsRtpSdpPublisherNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            *m_terminalFailure);
    }
    auto input = m_pendingPlan
        ? ::media::Result<std::optional<MediaBufferRef>>::success(m_pendingPlan)
        : tryPopInputOptional(context, "plan");
    if (!input) return failTerminal(input.error());
    if (!input.value()) {
        MediaChannel* channel = context.findInputChannel(nodeId(), "plan");
        if (channel && channel->aborted()) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "MP2T SDP publisher plan input was aborted"));
        }
        if (channel && channel->closed()) {
            return m_lastPublishedGeneration
                ? processFinished()
                : failTerminal(::media::ErrorInfo::notInitialized(
                      "MP2T SDP publisher plan input closed before publication"));
        }
        return processWaiting();
    }
    m_pendingPlan = *input.value();
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            input.value()->get())) {
        m_pendingPlan.reset();
        if ((control->controlKind() == MediaControlBufferKind::Eof ||
             control->controlKind() == MediaControlBufferKind::Flush) &&
            control->generation() && m_completedPurge &&
            *control->generation() == m_completedPurge->oldGeneration)
            return processProgress();
        auto activation = m_authority->currentActivation();
        if (control->controlKind() != MediaControlBufferKind::Abort &&
            (!activation || (control->generation()
                ? *control->generation() != activation.value().generation
                : m_streamSet != MediaTranscodeStreamSet::VideoOnly)))
            return failTerminal(::media::ErrorInfo::invalidArgument("SDP terminal requires exact active generation"));
        switch (control->controlKind()) {
        case MediaControlBufferKind::Eof:
            // EOS belongs to one generation; only channel closure ends the node.
            return processProgress();
        case MediaControlBufferKind::Flush:
            return processProgress();
        case MediaControlBufferKind::Abort:
            return failTerminal(::media::ErrorInfo::cancelled(
                "MP2T SDP publisher received abort"));
        default:
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "MP2T SDP publisher requires a runtime plan"));
        }
    }
    const auto* runtime =
        dynamic_cast<const MediaProjectMpegTsRuntimePlanBuffer*>(
            input.value()->get());
    if (!runtime || runtime->sessionKey() != m_plannedSession ||
        runtime->streamSet() != m_streamSet || !m_authority ||
        m_authority->sessionKey() != m_plannedSession ||
        m_authority->streamSet() != m_streamSet ||
        !m_authority->sharedNtpEpoch()) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "MP2T SDP publisher runtime authority is incomplete"));
    }
    const auto* rtpPlan = std::get_if<MediaMpegTsRtpOutputPlan>(
        &runtime->outputPlan().transport);
    if (!rtpPlan) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "MP2T SDP publisher requires an RTP transport plan"));
    }
    if (m_completedPurge && runtime->activation().generation == m_completedPurge->oldGeneration) {
        m_pendingPlan.reset();
        return processProgress();
    }
    auto currentActivation = m_authority->currentActivation();
    if (!currentActivation) {
        auto commit = m_authority->reserveCommit(runtime->activation().generation);
        if (!commit && commit.error().code == ::media::ErrorCode::Cancelled) return processWaiting();
        return failTerminal(currentActivation.error());
    }
    if (runtime->activation().generation < currentActivation.value().generation)
        return processWaiting();
    if (currentActivation.value() != runtime->activation()) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "MP2T SDP publisher activation differs from its authority"));
    }
    if (m_lastPublishedGeneration &&
        runtime->activation().generation <= *m_lastPublishedGeneration) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "MP2T SDP publisher requires strictly increasing generations"));
    }
    const auto sharedNtpEpoch = m_authority->sharedNtpEpoch();
    auto description = MediaMpegTsRtpSdpDescription::create(
        *rtpPlan, *sharedNtpEpoch,
        runtime->activation());
    if (!description) return failTerminal(description.error());
    auto serialized = description.value().serialize();
    if (!serialized) return failTerminal(serialized.error());
    auto outputCommit = m_authority->reserveCommit(
        runtime->activation().generation);
    if (!outputCommit) {
        return outputCommit.error().code == ::media::ErrorCode::Cancelled
            ? processWaiting()
            : failTerminal(outputCommit.error());
    }
    MediaAtomicUtf8FilePublisher publisher(*m_replacePort);
    auto published = publisher.publish(
        description.value().path(), serialized.value());
    if (!published) return failTerminal(published.error());
    m_lastPublishedGeneration = runtime->activation().generation;
    m_pendingPlan.reset();
    return processProgress();
}

void MediaMpegTsRtpSdpPublisherNode::resetState() noexcept
{
    m_pendingPlan.reset();
    m_terminalFailure.reset();
    m_lastPublishedGeneration.reset();
}

::media::Status MediaMpegTsRtpSdpPublisherNode::flush(
    MediaGraphExecutionContext& context)
{
    cancelPendingOutputTransfer();
    return FFmpegNodeRuntime::flush(context);
}

::media::Status MediaMpegTsRtpSdpPublisherNode::stop(
    MediaGraphExecutionContext& context)
{
    m_generationPurge->stop();
    resetState();
    return FFmpegNodeRuntime::stop(context);
}

void MediaMpegTsRtpSdpPublisherNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    m_lastPublishedGeneration.reset();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "MP2T SDP publisher was aborted");
    }
    m_generationPurge->stop();
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
