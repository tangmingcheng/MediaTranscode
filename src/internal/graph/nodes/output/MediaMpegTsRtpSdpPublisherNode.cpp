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
    return valid ? FFmpegNodeRuntime::start(context) : valid;
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsRtpSdpPublisherNode::failTerminal(
    ::media::ErrorInfo error)
{
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    return ::media::Result<MediaNodeProcessResult>::failure(
        *m_terminalFailure);
}

::media::Result<MediaNodeProcessResult>
MediaMpegTsRtpSdpPublisherNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            *m_terminalFailure);
    }
    auto input = tryPopInputOptional(context, "plan");
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
    if (const auto* control = dynamic_cast<const MediaControlBuffer*>(
            input.value()->get())) {
        switch (control->controlKind()) {
        case MediaControlBufferKind::Eof:
            return m_lastPublishedGeneration
                ? processFinished()
                : failTerminal(::media::ErrorInfo::notInitialized(
                      "MP2T SDP publisher reached EOF before publication"));
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
    auto currentActivation = m_authority->currentActivation();
    if (!currentActivation ||
        currentActivation.value() != runtime->activation()) {
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
            ? processProgress()
            : failTerminal(outputCommit.error());
    }
    MediaAtomicUtf8FilePublisher publisher(*m_replacePort);
    auto published = publisher.publish(
        description.value().path(), serialized.value());
    if (!published) return failTerminal(published.error());
    m_lastPublishedGeneration = runtime->activation().generation;
    return processProgress();
}

void MediaMpegTsRtpSdpPublisherNode::resetState() noexcept
{
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
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
