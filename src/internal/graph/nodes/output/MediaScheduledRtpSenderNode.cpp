#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include "internal/graph/nodes/output/MediaScheduledRtpOpenTransaction.h"
#include "internal/graph/nodes/output/MediaScheduledRtpSenderMaterializer.h"
#include "internal/graph/runtime/buffer/FFmpegCodecContextBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaPlaybackEpochActivatedBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"
#include "internal/graph/sync/MediaScheduledAccessUnit.h"

#include <new>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

using ProcessResult = ::media::Result<MediaNodeProcessResult>;

} // namespace

MediaScheduledRtpSenderNode::MediaScheduledRtpSenderNode(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey plannedGroupKey,
    MediaScheduledRtpOutputPlan outputPlan,
    MediaSeparateRtpSdpRuntimePlan sdpPlan,
    MediaScheduledRtpSenderNodeDependencies dependencies)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaScheduledRtpSenderNode"),
      m_plannedGroupKey(std::move(plannedGroupKey)),
      m_outputPlan(std::move(outputPlan)),
      m_sdpPlan(std::move(sdpPlan)),
      m_dependencies(std::move(dependencies)),
      m_generationState(std::make_shared<MediaProtocolOutputGenerationState>(
          m_outputPlan.stream == MediaScheduledStream::Video
              ? "rtp_video_output_generation_state"
              : "rtp_audio_output_generation_state"))
{
}

std::string_view
MediaScheduledRtpSenderNode::generationPurgeIdentity() const noexcept
{
    return m_generationState->plannedIdentity();
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
MediaScheduledRtpSenderNode::generationPurgeTarget() const noexcept
{
    return m_generationState;
}

::media::Result<std::unique_ptr<MediaScheduledRtpSenderNode>>
MediaScheduledRtpSenderNode::create(
    MediaNodeId nodeId,
    MediaAvSyncGroupKey plannedGroupKey,
    MediaScheduledRtpOutputPlan outputPlan,
    MediaSeparateRtpSdpRuntimePlan sdpPlan,
    MediaScheduledRtpSenderNodeDependencies dependencies)
{
    using NodeResult =
        ::media::Result<std::unique_ptr<MediaScheduledRtpSenderNode>>;
    const auto expectedStream = outputPlan.stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    if (!nodeId.isValid() || !plannedGroupKey.valid() ||
        !dependencies.syncGroup ||
        dependencies.syncGroup->key() != plannedGroupKey ||
        !dependencies.transportFactory || !dependencies.packetizerFactory ||
        outputPlan.packetization.streamKind() != expectedStream ||
        outputPlan.ssrc == 0 || outputPlan.clockRate <= 0 ||
        outputPlan.cname.empty() || outputPlan.cname != sdpPlan.cname ||
        outputPlan.senderLead.nanoseconds() <= 0 ||
        outputPlan.senderReportInterval.nanoseconds() <= 0) {
        return NodeResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender requires one complete planner-owned stream and exact sync group"));
    }
    try {
        return NodeResult::success(
            std::unique_ptr<MediaScheduledRtpSenderNode>(
                new MediaScheduledRtpSenderNode(
                    nodeId, std::move(plannedGroupKey), std::move(outputPlan),
                    std::move(sdpPlan), std::move(dependencies))));
    } catch (const std::bad_alloc&) {
        return NodeResult::failure(
            ::media::ErrorInfo::allocationFailed(
                "MediaScheduledRtpSenderNode"));
    }
}

MediaNodeKind MediaScheduledRtpSenderNode::staticKind() noexcept
{
    return MediaNodeKind::ScheduledRtpSender;
}

::media::Status MediaScheduledRtpSenderNode::start(
    MediaGraphExecutionContext& context)
{
    resetGenerationState();
    m_terminalFailure.reset();
    auto valid = validatePorts(context);
    if (!valid) return valid;
    return FFmpegNodeRuntime::start(context);
}

::media::Status MediaScheduledRtpSenderNode::validatePorts(
    MediaGraphExecutionContext& context) const
{
    const auto expectedStream = m_outputPlan.stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    const MediaChannel* epoch = context.findInputChannel(nodeId(), "epoch");
    const MediaChannel* codec = context.findInputChannel(nodeId(), "codec");
    const MediaChannel* scheduled = context.findInputChannel(nodeId(), "scheduled");
    const MediaChannel* description =
        context.findOutputChannel(nodeId(), "description");
    if (context.inputChannels(nodeId()).size() != 3 ||
        context.outputChannels(nodeId()).size() != 1 ||
        !epoch || !codec || !scheduled || !description ||
        codec->binding().streamKind != expectedStream ||
        scheduled->binding().streamKind != expectedStream ||
        description->binding().streamKind != MediaStreamKind::Metadata ||
        description->policy().queuePolicy.overflowPolicy !=
            MediaQueueOverflowPolicy::BlockProducer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender requires exact epoch, codec, scheduled, and blocking description ports"));
    }
    return ::media::Status::success();
}

::media::Result<bool> MediaScheduledRtpSenderNode::acquireActivation(
    MediaGraphExecutionContext& context)
{
    if (m_activation) return ::media::Result<bool>::success(false);
    auto input = tryPopInputOptional(context, "epoch");
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    const auto* activated = dynamic_cast<const MediaPlaybackEpochActivatedBuffer*>(
        input.value()->get());
    if (!activated || activated->groupKey() != m_plannedGroupKey ||
        m_dependencies.syncGroup->lifecycleState() !=
            MediaAvSyncGroupRuntime::LifecycleState::Active) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender rejects an invalid playback activation"));
    }
    auto groupEpoch = m_dependencies.syncGroup->playbackEpoch();
    if (!groupEpoch || groupEpoch.value() != activated->epoch() ||
        !m_dependencies.syncGroup->sharedNtpEpoch()) {
        return ::media::Result<bool>::failure(
            groupEpoch ? ::media::ErrorInfo::invalidArgument(
                             "Scheduled RTP activation differs from the registered group")
                       : groupEpoch.error());
    }
    m_epoch = groupEpoch.value();
    const auto transition = m_generationState->activationTransitionSequence(
        m_epoch->generation);
    if (auto permitted = m_generationState->permitActivatedGeneration(
            m_epoch->generation, transition.value_or(0)); !permitted) {
        return ::media::Result<bool>::failure(permitted.error());
    }
    m_activation = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Result<bool> MediaScheduledRtpSenderNode::acquireCodec(
    MediaGraphExecutionContext& context)
{
    if (m_codec) return ::media::Result<bool>::success(false);
    auto input = tryPopInputOptional(context, "codec");
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(
        input.value()->get());
    if (!codec || !codec->context()) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender requires runtime encoder metadata"));
    }
    m_codec = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Status MediaScheduledRtpSenderNode::openSender()
{
    if (m_sender || !m_epoch || !m_activation || !m_codec) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender cannot open before activation and codec metadata"));
    }
    const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(m_codec.get());
    auto sharedNtpEpoch = m_dependencies.syncGroup->sharedNtpEpoch();
    if (!codec || !codec->context() || !sharedNtpEpoch) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender lost encoder metadata or shared NTP epoch"));
    }
    auto materialized = MediaScheduledRtpSenderMaterializer::materialize(
        m_outputPlan, m_sdpPlan, *codec->context(), *sharedNtpEpoch, *m_epoch);
    if (!materialized) {
        return ::media::Status::failure(materialized.error());
    }
    MediaBufferRef description =
        materialized.value().releaseDescription();
    if (!m_transport) {
        auto opened = MediaScheduledRtpOpenTransaction::open(
            materialized.value().releaseTransportConfig(),
            materialized.value().releaseSenderConfig(),
            *m_dependencies.transportFactory,
            *m_dependencies.packetizerFactory);
        if (!opened) return ::media::Status::failure(opened.error());
        m_transport = opened.value().releaseTransport();
        m_sender = opened.value().releaseSender();
    } else {
        auto* transport = m_transport.get();
        auto sender = ScheduledRtpSenderSession::create(
            materialized.value().releaseSenderConfig(),
            [transport](std::span<const std::uint8_t> datagram, std::size_t) {
                return transport->sendRtp(datagram);
            },
            [transport](std::span<const std::uint8_t> datagram) {
                return transport->sendRtcp(datagram);
            },
            *m_dependencies.packetizerFactory);
        if (!sender) return ::media::Status::failure(sender.error());
        if (auto opened = sender.value()->open(); !opened) return opened;
        m_sender = std::move(sender).value();
    }
    m_description = std::move(description);
    return ::media::Status::success();
}

::media::Result<MediaNodeProcessResult>
MediaScheduledRtpSenderNode::emitDescription(
    MediaGraphExecutionContext& context)
{
    if (m_descriptionEmitted) return processProgress();
    if (!m_description) {
        return failTerminal(::media::ErrorInfo::notInitialized(
            "Scheduled RTP sender has no opened description"));
    }
    m_descriptionEmitted = true;
    return processProgress(emitOutput(context, "description", m_description));
}

::media::Result<MediaNodeProcessResult>
MediaScheduledRtpSenderNode::processScheduledInput(
    MediaGraphExecutionContext& context)
{
    MediaChannel* channel = context.findInputChannel(nodeId(), "scheduled");
    if (!channel) {
        return failTerminal(::media::ErrorInfo::notInitialized(
            "Scheduled RTP sender has no scheduled input"));
    }
    if (!m_epoch) {
        return failTerminal(::media::ErrorInfo::notInitialized(
            "Scheduled RTP sender has no activated generation"));
    }
    if (auto permitted = validateOutputPermit(m_epoch->generation);
        !permitted) {
        return processWaiting();
    }
    auto now = m_dependencies.syncGroup->clock()->now();
    if (!now) return failTerminal(now.error());
    auto report = m_sender->dispatchSenderReport(now.value());
    if (!report) {
        if (report.error().code == ::media::ErrorCode::WouldBlock) {
            auto retryDeadline = now.value().checkedAdd(
                m_outputPlan.senderReportInterval);
            if (!retryDeadline) return failTerminal(retryDeadline.error());
            return ProcessResult::success(MediaNodeProcessResult::waitingUntil(
                m_plannedGroupKey, retryDeadline.value()));
        }
        return failTerminal(report.error());
    }
    if (report.value().kind() == ScheduledRtcpDispatchKind::Sent) {
        return processProgress();
    }
    const MediaRunningTime nextReportDeadline = report.value().nextDeadline();
    auto input = tryPopInputOptional(context, "scheduled");
    if (!input) return failTerminal(input.error());
    if (!input.value()) {
        if (channel->aborted()) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "Scheduled RTP sender input was aborted"));
        }
        if (channel->closed()) {
            closeSession();
            return processFinished();
        }
        return ProcessResult::success(MediaNodeProcessResult::waitingUntil(
            m_plannedGroupKey, nextReportDeadline));
    }
    const auto* control = dynamic_cast<const MediaControlBuffer*>(
        input.value()->get());
    if (control) {
        switch (control->controlKind()) {
        case MediaControlBufferKind::Eof:
            closeSession();
            return processFinished();
        case MediaControlBufferKind::Flush:
            resetGenerationSession();
            return processProgress();
        case MediaControlBufferKind::Abort:
            m_dependencies.syncGroup->markAborted();
            return failTerminal(::media::ErrorInfo::cancelled(
                "Scheduled RTP sender received abort"));
        case MediaControlBufferKind::Unknown:
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender rejects unknown control"));
        }
    }
    const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(
        input.value()->get());
    if (!scheduled || scheduled->stream() != m_outputPlan.stream) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender rejects a mismatched scheduled access unit"));
    }
    if (scheduled->generation() != m_epoch->generation) {
        if (scheduled->generation() < m_epoch->generation) {
            return processProgress();
        }
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender rejects a future scheduled generation"));
    }
    if (auto permitted = validateOutputPermit(scheduled->generation());
        !permitted) {
        return processWaiting();
    }
    auto actualLead = scheduled->dispatchOnMaster().checkedSubtract(
        scheduled->emitOnMaster());
    if (!actualLead || actualLead.value() != m_outputPlan.senderLead) {
        return failTerminal(actualLead
            ? ::media::ErrorInfo::invalidArgument(
                  "Scheduled RTP access unit does not preserve planned sender lead")
            : actualLead.error());
    }
    const AVPacket* packet = FFmpegPacketView::packet(scheduled->media());
    if (!packet) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP access unit has no packet payload"));
    }
    auto sent = m_sender->sendAccessUnit(
        *packet, scheduled->presentationOnMaster());
    if (!sent) return failTerminal(sent.error());
    return processProgress();
}

::media::Result<MediaNodeProcessResult> MediaScheduledRtpSenderNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) return ProcessResult::failure(*m_terminalFailure);
    const auto snapshot =
        m_dependencies.syncGroup->epochTransitionSnapshot();
    if (snapshot.poisoned) {
        return failTerminal(::media::ErrorInfo::cancelled(
            "Scheduled RTP sender output authority is poisoned"));
    }
    if (!snapshot.outputPermitted || !snapshot.playbackEpoch) {
        cancelPendingOutputTransfer();
        return processWaiting();
    }
    if (m_epoch &&
        m_epoch->generation != snapshot.playbackEpoch->generation) {
        resetGenerationSession();
    }
    auto activation = acquireActivation(context);
    if (!activation) return failTerminal(activation.error());
    auto codec = acquireCodec(context);
    if (!codec) return failTerminal(codec.error());
    if (!m_sender) {
        if (!m_activation || !m_codec) {
            MediaChannel* epoch = context.findInputChannel(nodeId(), "epoch");
            MediaChannel* metadata = context.findInputChannel(nodeId(), "codec");
            if ((epoch && (epoch->closed() || epoch->aborted())) ||
                (metadata && (metadata->closed() || metadata->aborted()))) {
                return failTerminal(::media::ErrorInfo::notInitialized(
                    "Scheduled RTP sender lost required activation or codec input"));
            }
            return (activation.value() || codec.value())
                ? processProgress()
                : processWaiting();
        }
        if (auto opened = openSender(); !opened) {
            return failTerminal(opened.error());
        }
        return emitDescription(context);
    }
    if (!m_descriptionEmitted) return emitDescription(context);
    if (m_dependencies.syncGroup->lifecycleState() !=
        MediaAvSyncGroupRuntime::LifecycleState::Active) {
        return failTerminal(::media::ErrorInfo::cancelled(
            "Scheduled RTP sender sync group is no longer active"));
    }
    return processScheduledInput(context);
}

::media::Status MediaScheduledRtpSenderNode::validateOutputPermit(
    std::uint64_t generation) const
{
    const auto snapshot =
        m_dependencies.syncGroup->epochTransitionSnapshot();
    if (snapshot.poisoned || !snapshot.outputPermitted ||
        !snapshot.playbackEpoch ||
        snapshot.playbackEpoch->generation != generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled(
                "Scheduled RTP output permit is closed for this generation"));
    }
    return m_generationState->validateCommitGeneration(generation);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledRtpSenderNode::failTerminal(::media::ErrorInfo error)
{
    if (error.code == ::media::ErrorCode::WouldBlock) {
        error = ::media::ErrorInfo::ioFailure(
            "Scheduled RTP sender cannot retry a consumed access unit after output pressure: " +
                error.message,
            error.nativeCode);
    }
    if (!m_terminalFailure) m_terminalFailure = std::move(error);
    closeSession();
    return ProcessResult::failure(*m_terminalFailure);
}

void MediaScheduledRtpSenderNode::closeSession() noexcept
{
    m_sender.reset();
    if (m_transport) {
        (void)m_transport->close();
        m_transport.reset();
    }
}

void MediaScheduledRtpSenderNode::resetGenerationSession() noexcept
{
    cancelPendingOutputTransfer();
    m_sender.reset();
    m_activation.reset();
    m_description.reset();
    m_epoch.reset();
    m_descriptionEmitted = false;
}

void MediaScheduledRtpSenderNode::resetGenerationState() noexcept
{
    resetGenerationSession();
    closeSession();
    m_codec.reset();
    m_generationState->resetLifecycle();
}

::media::Status MediaScheduledRtpSenderNode::flush(
    MediaGraphExecutionContext& context)
{
    resetGenerationState();
    m_terminalFailure.reset();
    return FFmpegNodeRuntime::flush(context);
}

::media::Status MediaScheduledRtpSenderNode::stop(
    MediaGraphExecutionContext& context)
{
    resetGenerationState();
    m_terminalFailure.reset();
    return FFmpegNodeRuntime::stop(context);
}

void MediaScheduledRtpSenderNode::abort(
    MediaGraphExecutionContext& context) noexcept
{
    if (m_dependencies.syncGroup) m_dependencies.syncGroup->markAborted();
    closeSession();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Scheduled RTP sender was aborted");
    }
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
