#include "internal/graph/nodes/output/MediaScheduledRtpSenderNode.h"

#include "internal/graph/nodes/output/MediaRtpSenderDescriptionBuffer.h"
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
    MediaProtocolOutputSessionKey plannedSessionKey,
    MediaScheduledRtpOutputPlan outputPlan,
    MediaSeparateRtpSdpRuntimePlan sdpPlan,
    MediaScheduledRtpSenderNodeDependencies dependencies)
    : FFmpegNodeRuntime(nodeId, staticKind(), "MediaScheduledRtpSenderNode"),
      m_plannedSessionKey(std::move(plannedSessionKey)),
      m_outputPlan(std::move(outputPlan)),
      m_sdpPlan(std::move(sdpPlan)),
      m_dependencies(std::move(dependencies)),
      m_sessionState(
          std::make_shared<MediaScheduledRtpGenerationSessionState>()),
      m_generationState(std::make_shared<MediaProtocolOutputGenerationState>(
          m_outputPlan.stream == MediaScheduledStream::Video
              ? "rtp_video_output_generation_state"
              : "rtp_audio_output_generation_state",
          m_sessionState))
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
    MediaProtocolOutputSessionKey plannedSessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaScheduledRtpOutputPlan outputPlan,
    MediaSeparateRtpSdpRuntimePlan sdpPlan,
    MediaScheduledRtpSenderNodeDependencies dependencies)
{
    using NodeResult =
        ::media::Result<std::unique_ptr<MediaScheduledRtpSenderNode>>;
    const auto expectedStream = outputPlan.stream == MediaScheduledStream::Video
        ? MediaStreamKind::Video
        : MediaStreamKind::Audio;
    if (!nodeId.isValid() || !plannedSessionKey.valid() ||
        !dependencies.authority ||
        dependencies.authority->sessionKey() != plannedSessionKey ||
        dependencies.authority->streamSet() != streamSet ||
        (streamSet == MediaTranscodeStreamSet::VideoOnly &&
         outputPlan.stream != MediaScheduledStream::Video) ||
        !dependencies.transportFactory || !dependencies.packetizerFactory ||
        outputPlan.packetization.streamKind() != expectedStream ||
        outputPlan.ssrc == 0 || outputPlan.clockRate <= 0 ||
        outputPlan.cname.empty() || outputPlan.cname != sdpPlan.cname ||
        outputPlan.senderLead.nanoseconds() <= 0 ||
        outputPlan.senderReportInterval.nanoseconds() <= 0) {
        return NodeResult::failure(
            ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender requires one complete planner-owned stream and exact protocol authority"));
    }
    try {
        return NodeResult::success(
            std::unique_ptr<MediaScheduledRtpSenderNode>(
                new MediaScheduledRtpSenderNode(
                    nodeId, std::move(plannedSessionKey), std::move(outputPlan),
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
    const MediaChannel* activation = context.findInputChannel(
        nodeId(), "activation");
    const MediaChannel* codec = context.findInputChannel(nodeId(), "codec");
    const MediaChannel* scheduled = context.findInputChannel(nodeId(), "scheduled");
    const MediaChannel* description =
        context.findOutputChannel(nodeId(), "description");
    if (context.inputChannels(nodeId()).size() != 3 ||
        context.outputChannels(nodeId()).size() != 1 ||
        !activation || !codec || !scheduled || !description ||
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
    auto input = tryPopInputOptional(context, "activation");
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    auto activation = m_dependencies.authority->validateActivation(
        *input.value());
    if (!activation) return ::media::Result<bool>::failure(activation.error());
    auto permitted = m_generationState->permitActivatedGeneration(
            *m_dependencies.authority, activation.value().generation,
            activation.value().completedTransitionSequence);
    if (!permitted) {
        return ::media::Result<bool>::failure(permitted.error());
    }
    m_sessionState->activationFacts = activation.value();
    m_sessionState->activation = std::move(*input.value());
    m_sessionState->generation.store(
        activation.value().generation, std::memory_order_release);
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

::media::Status MediaScheduledRtpSenderNode::openSender(
    const AVPacket* codecConfigurationAccessUnit)
{
    if (m_sessionState->sender || !m_sessionState->activationFacts ||
        !m_sessionState->activation || !m_codec) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender cannot open before activation and codec metadata"));
    }
    const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(m_codec.get());
    auto sharedNtpEpoch = m_dependencies.authority->sharedNtpEpoch();
    if (!codec || !codec->context() || !sharedNtpEpoch) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender lost encoder metadata or shared NTP epoch"));
    }
    auto materialized = MediaScheduledRtpSenderMaterializer::materialize(
        m_outputPlan, m_sdpPlan, *codec->context(),
        codecConfigurationAccessUnit, *sharedNtpEpoch,
        *m_sessionState->activationFacts);
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
        m_sessionState->sender = opened.value().releaseSender();
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
        m_sessionState->sender = std::move(sender).value();
    }
    m_sessionState->description = std::move(description);
    return ::media::Status::success();
}

::media::Result<bool>
MediaScheduledRtpSenderNode::acquireCodecConfigurationAccessUnit(
    MediaGraphExecutionContext& context,
    std::uint64_t activeGeneration)
{
    const auto mode = m_outputPlan.packetization.packetizationMode();
    if ((mode != MediaScheduledRtpPacketizationMode::H264AnnexB &&
         mode != MediaScheduledRtpPacketizationMode::HevcAnnexB) ||
        m_codecConfigurationAccessUnit) {
        return ::media::Result<bool>::success(false);
    }
    const auto* codec = dynamic_cast<const FFmpegCodecContextBuffer*>(
        m_codec.get());
    if (!m_codec) return ::media::Result<bool>::success(false);
    if (!codec || !codec->context()) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::notInitialized(
                "Video RTP bootstrap requires encoder metadata"));
    }
    if (codec->context()->extradata && codec->context()->extradata_size > 0) {
        return ::media::Result<bool>::success(false);
    }
    auto input = tryPopInputOptional(context, "scheduled");
    if (!input) return ::media::Result<bool>::failure(input.error());
    if (!input.value()) return ::media::Result<bool>::success(false);
    const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(
        input.value()->get());
    const AVPacket* packet = scheduled
        ? FFmpegPacketView::packet(scheduled->media())
        : nullptr;
    if (!scheduled || scheduled->stream() != MediaScheduledStream::Video ||
        scheduled->generation() != activeGeneration || !packet ||
        !packet->data || packet->size <= 0) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video RTP bootstrap requires the first current-generation access unit"));
    }
    m_codecConfigurationAccessUnit = std::move(*input.value());
    return ::media::Result<bool>::success(true);
}

::media::Result<MediaNodeProcessResult>
MediaScheduledRtpSenderNode::emitDescription(
    MediaGraphExecutionContext& context)
{
    MediaBufferRef description;
    std::optional<::media::ErrorInfo> failure;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        if (m_sessionState->descriptionEmitted) return processProgress();
        if (!m_sessionState->description) {
            failure = ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender has no opened description");
        } else {
            description = m_sessionState->description;
        }
    }
    if (failure) return failTerminal(std::move(*failure));
    return processProgress(emitOutput(
        context, "description", description));
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
    auto now = m_dependencies.authority->now();
    if (!now) return failTerminal(now.error());
    const auto activeGeneration =
        m_sessionState->generation.load(std::memory_order_acquire);
    if (activeGeneration == 0) return processWaiting();
    auto reportDisposition =
        m_generationState->classifyGeneration(activeGeneration);
    if (!reportDisposition) {
        return failTerminal(reportDisposition.error());
    }
    if (reportDisposition.value() !=
        MediaProtocolOutputGenerationState::GenerationDisposition::Current) {
        return reportDisposition.value() ==
                MediaProtocolOutputGenerationState::GenerationDisposition::Old
            ? processWaiting()
            : failTerminal(::media::ErrorInfo::invalidArgument(
                  "Scheduled RTP sender report rejects a future generation"));
    }
    std::optional<MediaRunningTime> nextReportDeadline;
    std::optional<::media::ErrorInfo> reportFailure;
    {
        auto reportReservation = m_generationState->reserveCommit(
            *m_dependencies.authority, activeGeneration);
        if (!reportReservation) {
            return reportReservation.error().code ==
                    ::media::ErrorCode::Cancelled
                ? processWaiting()
                : failTerminal(reportReservation.error());
        }
        if (!m_sessionState->activationFacts ||
            m_sessionState->activationFacts->generation != activeGeneration ||
            !m_sessionState->sender) {
            reportFailure = ::media::ErrorInfo::notInitialized(
                "Scheduled RTP sender has no exact activated session");
        } else {
            auto report =
                m_sessionState->sender->dispatchSenderReport(now.value());
            if (!report) {
                if (report.error().code == ::media::ErrorCode::Cancelled) {
                    return processWaiting();
                }
                if (report.error().code == ::media::ErrorCode::WouldBlock) {
                    auto retryDeadline = now.value().checkedAdd(
                        m_outputPlan.senderReportInterval);
                    if (!retryDeadline) {
                        reportFailure = retryDeadline.error();
                    } else {
                        return ProcessResult::success(
                            {MediaNodeProcessState::Waiting,
                             m_dependencies.authority->deadlineWait(
                                 retryDeadline.value(),
                                 MediaNodeDeadlineWakePolicy::InputOrDeadline)});
                    }
                } else {
                    reportFailure = report.error();
                }
            } else if (report.value().kind() ==
                       ScheduledRtcpDispatchKind::Sent) {
                return processProgress();
            } else {
                nextReportDeadline = report.value().nextDeadline();
            }
        }
    }
    if (reportFailure) return failTerminal(std::move(*reportFailure));
    std::optional<MediaBufferRef> input;
    if (m_codecConfigurationAccessUnit) {
        input = std::move(m_codecConfigurationAccessUnit);
    } else {
        auto popped = tryPopInputOptional(context, "scheduled");
        if (!popped) return failTerminal(popped.error());
        input = std::move(popped).value();
    }
    if (!input) {
        if (channel->aborted()) {
            return failTerminal(::media::ErrorInfo::cancelled(
                "Scheduled RTP sender input was aborted"));
        }
        if (channel->closed()) {
            closeSession();
            return processFinished();
        }
        return ProcessResult::success(
            {MediaNodeProcessState::Waiting,
             m_dependencies.authority->deadlineWait(
                 *nextReportDeadline,
                 MediaNodeDeadlineWakePolicy::InputOrDeadline)});
    }
    const auto* control = dynamic_cast<const MediaControlBuffer*>(
        input->get());
    if (control) {
        switch (control->controlKind()) {
        case MediaControlBufferKind::Eof:
            closeSession();
            return processFinished();
        case MediaControlBufferKind::Flush:
            {
                auto mutation = m_generationState->reserveSessionMutation();
                resetGenerationSession();
            }
            return processProgress();
        case MediaControlBufferKind::Abort:
            m_dependencies.authority->markAborted();
            return failTerminal(::media::ErrorInfo::cancelled(
                "Scheduled RTP sender received abort"));
        case MediaControlBufferKind::Unknown:
            return failTerminal(::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender rejects unknown control"));
        }
    }
    const auto* scheduled = dynamic_cast<const MediaScheduledAccessUnit*>(
        input->get());
    if (!scheduled || scheduled->stream() != m_outputPlan.stream) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender rejects a mismatched scheduled access unit"));
    }
    auto disposition =
        m_generationState->classifyGeneration(scheduled->generation());
    if (!disposition) return failTerminal(disposition.error());
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Old) {
        return processProgress();
    }
    if (disposition.value() ==
        MediaProtocolOutputGenerationState::GenerationDisposition::Future) {
        return failTerminal(::media::ErrorInfo::invalidArgument(
            "Scheduled RTP sender rejects a future scheduled generation"));
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
    std::optional<::media::ErrorInfo> sendFailure;
    {
        auto sendReservation = m_generationState->reserveCommit(
            *m_dependencies.authority, scheduled->generation());
        if (!sendReservation) {
            return sendReservation.error().code ==
                    ::media::ErrorCode::Cancelled
                ? processProgress()
                : failTerminal(sendReservation.error());
        }
        if (!m_sessionState->activationFacts || !m_sessionState->sender ||
            scheduled->generation() !=
                m_sessionState->activationFacts->generation) {
            if (scheduled->generation() < activeGeneration) {
                return processProgress();
            }
            sendFailure = ::media::ErrorInfo::invalidArgument(
                "Scheduled RTP sender rejects a future scheduled generation");
        } else {
            auto sent = m_sessionState->sender->sendAccessUnit(
                *packet, scheduled->presentationOnMaster());
            if (!sent) sendFailure = sent.error();
        }
    }
    if (sendFailure) return failTerminal(std::move(*sendFailure));
    return processProgress();
}

::media::Result<MediaNodeProcessResult> MediaScheduledRtpSenderNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) return ProcessResult::failure(*m_terminalFailure);
    auto activation = acquireActivation(context);
    if (!activation) return failTerminal(activation.error());
    auto codec = acquireCodec(context);
    if (!codec) return failTerminal(codec.error());
    const auto activeGeneration =
        m_sessionState->generation.load(std::memory_order_acquire);
    if (activeGeneration == 0) {
        return (activation.value() || codec.value())
            ? processProgress()
            : processWaiting();
    }
    auto disposition =
        m_generationState->classifyGeneration(activeGeneration);
    if (!disposition) return failTerminal(disposition.error());
    if (disposition.value() !=
        MediaProtocolOutputGenerationState::GenerationDisposition::Current) {
        return disposition.value() ==
                MediaProtocolOutputGenerationState::GenerationDisposition::Old
            ? processWaiting()
            : failTerminal(::media::ErrorInfo::invalidArgument(
                  "Scheduled RTP session rejects a future active generation"));
    }
    bool emitOpenedDescription = false;
    auto configuration = acquireCodecConfigurationAccessUnit(
        context, activeGeneration);
    if (!configuration) return failTerminal(configuration.error());
    const auto* configurationAccessUnit =
        dynamic_cast<const MediaScheduledAccessUnit*>(
            m_codecConfigurationAccessUnit.get());
    const AVPacket* configurationPacket = configurationAccessUnit
        ? FFmpegPacketView::packet(configurationAccessUnit->media())
        : nullptr;
    const auto* codecBuffer = dynamic_cast<const FFmpegCodecContextBuffer*>(
        m_codec.get());
    const auto packetizationMode =
        m_outputPlan.packetization.packetizationMode();
    const bool configurationRequired =
        (packetizationMode == MediaScheduledRtpPacketizationMode::H264AnnexB ||
         packetizationMode == MediaScheduledRtpPacketizationMode::HevcAnnexB) &&
        codecBuffer && codecBuffer->context() &&
        (!codecBuffer->context()->extradata ||
         codecBuffer->context()->extradata_size <= 0);
    if (configurationRequired && !configurationPacket) {
        return configuration.value() ? processProgress() : processWaiting();
    }
    std::optional<::media::ErrorInfo> sessionFailure;
    {
        auto sessionReservation = m_generationState->reserveCommit(
            *m_dependencies.authority, activeGeneration);
        if (!sessionReservation) {
            return sessionReservation.error().code ==
                    ::media::ErrorCode::Cancelled
                ? processWaiting()
                : failTerminal(sessionReservation.error());
        }
        if (!m_sessionState->activationFacts ||
            m_sessionState->activationFacts->generation != activeGeneration) {
            sessionFailure = ::media::ErrorInfo::internalError(
                "Scheduled RTP session generation differs from its current authority");
        } else if (!m_sessionState->sender) {
            if (!m_sessionState->activation || !m_codec) {
                MediaChannel* activationChannel =
                    context.findInputChannel(nodeId(), "activation");
                MediaChannel* metadata = context.findInputChannel(nodeId(), "codec");
                if ((activationChannel &&
                     (activationChannel->closed() ||
                      activationChannel->aborted())) ||
                    (metadata && (metadata->closed() || metadata->aborted()))) {
                    sessionFailure = ::media::ErrorInfo::notInitialized(
                        "Scheduled RTP sender lost required activation or codec input");
                } else {
                    return (activation.value() || codec.value())
                        ? processProgress()
                        : processWaiting();
                }
            } else if (auto opened = openSender(configurationPacket); !opened) {
                sessionFailure = opened.error();
            } else {
                emitOpenedDescription = true;
            }
        } else if (!m_sessionState->descriptionEmitted) {
            emitOpenedDescription = true;
        }
    }
    if (sessionFailure) return failTerminal(std::move(*sessionFailure));
    if (emitOpenedDescription) return emitDescription(context);
    return processScheduledInput(context);
}

::media::Result<MediaOutputCommitReservation>
MediaScheduledRtpSenderNode::reserveOutputCommit(
    const MediaBufferRef& buffer) const
{
    const auto* description =
        dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(buffer.get());
    if (!description) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    ::media::ErrorInfo::invalidArgument(
                        "Scheduled RTP output commit requires a typed sender description"));
    }
    auto disposition =
        m_generationState->classifyGeneration(description->generation());
    if (!disposition) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    disposition.error());
    }
    if (disposition.value() !=
        MediaProtocolOutputGenerationState::GenerationDisposition::Current) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    disposition.value() ==
                            MediaProtocolOutputGenerationState::
                                GenerationDisposition::Old
                        ? ::media::ErrorInfo::cancelled(
                              "Scheduled RTP drops an old sender description")
                        : ::media::ErrorInfo::invalidArgument(
                              "Scheduled RTP rejects a future sender description"));
    }
    auto reservation = m_generationState->reserveCommit(
        *m_dependencies.authority, description->generation());
    if (!reservation) {
        return ::media::Result<MediaOutputCommitReservation>::failure(
                    reservation.error());
    }
    return ::media::Result<MediaOutputCommitReservation>::success(
        MediaOutputCommitReservation::hold(
            std::move(reservation).value()));
}

::media::Status MediaScheduledRtpSenderNode::commitReservedOutput(
    const MediaBufferRef& buffer)
{
    const auto* description =
        dynamic_cast<const MediaRtpSenderDescriptionBuffer*>(buffer.get());
    if (!description || !m_sessionState->activationFacts ||
        description->generation() !=
            m_sessionState->activationFacts->generation) {
        return ::media::Status::failure(
            ::media::ErrorInfo::cancelled(
                "Scheduled RTP description commit differs from its exact session generation"));
    }
    m_sessionState->descriptionEmitted = true;
    return ::media::Status::success();
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
    std::unique_ptr<MediaRtpUdpSenderTransport> transport;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        m_sessionState->sender.reset();
        transport = std::move(m_transport);
    }
    if (transport) (void)transport->close();
}

void MediaScheduledRtpSenderNode::resetGenerationSession() noexcept
{
    cancelPendingOutputTransfer();
    m_sessionState->resetForGenerationPurge();
}

void MediaScheduledRtpSenderNode::resetGenerationState() noexcept
{
    std::unique_ptr<MediaRtpUdpSenderTransport> transport;
    {
        auto mutation = m_generationState->reserveSessionMutation();
        resetGenerationSession();
        m_sessionState->sender.reset();
        transport = std::move(m_transport);
    }
    if (transport) (void)transport->close();
    m_codec.reset();
    m_codecConfigurationAccessUnit.reset();
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
    if (m_dependencies.authority) m_dependencies.authority->markAborted();
    closeSession();
    if (!m_terminalFailure) {
        m_terminalFailure = ::media::ErrorInfo::cancelled(
            "Scheduled RTP sender was aborted");
    }
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
