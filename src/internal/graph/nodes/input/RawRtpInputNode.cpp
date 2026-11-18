#include "internal/graph/nodes/input/RawRtpInputNode.h"

#include "internal/graph/nodes/input/MediaRtpIngressNodePlanDecoder.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"
#include "internal/graph/runtime/lifecycle/MediaInputActivity.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/sync/MediaAvSyncGroupRuntime.h"
#include "internal/graph/runtime/resource/MediaGraphPayloadCreditLedger.h"
#include "internal/graph/runtime/session/MediaPreparedRtpSourceSession.h"

#include <chrono>
#include <algorithm>
#include <new>
#include <limits>
#include <string>
#include <utility>
#include <tuple>

namespace media::ffmpeg::graph {
namespace {

const char* discontinuityReasonName(
    MediaRtpDiscontinuityReason reason) noexcept
{
    switch (reason) {
    case MediaRtpDiscontinuityReason::SequenceGap:
        return "sequence_gap";
    case MediaRtpDiscontinuityReason::SsrcChanged:
        return "ssrc_changed";
    case MediaRtpDiscontinuityReason::PayloadTypeChanged:
        return "payload_type_changed";
    }
    return "unknown";
}

using RtpEvidenceDiagnostic = std::tuple<std::uint64_t, std::uint32_t, std::int64_t>;

void logRtpClockEvidence(MediaStreamKind stream, const RtpEvidenceDiagnostic& facts,
    std::int64_t observedAtNs)
{
    const auto& [generation, ssrc, reportAtNs] = facts;
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "rtp_clock_evidence stream=" + std::string(stream == MediaStreamKind::Video ? "video" : "audio") +
            " generation=" + std::to_string(generation) + " ssrc=" + std::to_string(ssrc) +
            " age_ms=" + std::to_string(observedAtNs >= reportAtNs
                ? (observedAtNs - reportAtNs) / 1'000'000 : -1));
}

} // namespace

RawRtpInputNode::RawRtpInputNode(MediaNodeId nodeId,
    std::optional<MediaRtpIngressBinding> ingressBinding)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RawRtpInputNode")
    , m_ingressBinding(std::move(ingressBinding))
    , m_ownerPurge(m_ingressBinding ? std::make_shared<MediaOwnerThreadGenerationPurge>() : nullptr)
{
}

RawRtpInputNode::RawRtpInputNode(
    MediaNodeId nodeId,
    MediaPreparedRealtimeInput prepared,
    std::optional<MediaRtpIngressBinding> ingressBinding)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RawRtpInputNode"),
      m_ingressBinding(std::move(ingressBinding)),
      m_ownerPurge(m_ingressBinding ? std::make_shared<MediaOwnerThreadGenerationPurge>() : nullptr),
      m_prepared(std::move(prepared)),
      m_requiresPreparedInput(true)
{
}

MediaNodeKind RawRtpInputNode::staticKind() noexcept
{
    return MediaNodeKind::RawRtpInput;
}

::media::Status RawRtpInputNode::bindPreparedSession(std::shared_ptr<MediaPreparedRtpSourceSession> source)
{
    if (!source || m_preparedSource || m_initialized || !m_requiresPreparedInput)
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Raw RTP requires one prepared source before initialization"));
    m_preparedSource = std::move(source);
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::start(MediaGraphExecutionContext& context)
{
    if (m_initialized) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RawRtpInputNode start requires a stopped receiver"));
    }
    if (auto status = FFmpegNodeRuntime::start(context); !status) {
        return status;
    }
    auto status = prepareReceiver(context);
    if (status && m_preparedSource)
        status = m_preparedSource->activateStream(m_config.streamKind,
            context.sharedNodeWakeup(nodeId()), context.inputActivity());
    if (status && m_ingressBinding) {
        m_syncGroup = context.findAvSyncGroup(m_ingressBinding->group);
        if (!m_syncGroup || !m_syncGroup->rtpIngressFrontiers() ||
            m_ingressBinding->stream != m_config.streamKind) {
            status = ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                "RTP ingress purge requires its exact planned source and stream"));
        } else {
            status = m_ownerPurge->start(context.sharedNodeWakeup(nodeId()));
        }
    }
    if (!status) {
        m_transport.close();
        resetState();
        FFmpegNodeRuntime::abort(context);
    }
    return status;
}

::media::Result<MediaNodeProcessResult> RawRtpInputNode::process(
    MediaGraphExecutionContext& context)
{
    if (m_ownerPurge) {
        if (const auto purge = m_ownerPurge->pending()) {
            auto ledger = context.payloadCreditLedger();
            auto status = ledger ? ledger->cancelProducerWaiter(
                {nodeId(), m_config.streamKind, MediaPayloadKind::Packet})
                : ::media::Status::failure(::media::ErrorInfo::notInitialized(
                      "RTP owner purge requires its payload credit ledger"));
            if (!status) {
                const auto completed = m_ownerPurge->complete(*purge, status);
                return processProgress(completed ? status : completed);
            }
            if (const auto* pending = pendingOutputBuffer()) {
                const auto* control = dynamic_cast<const MediaControlBuffer*>(pending);
                if (pending->payloadKind() == MediaPayloadKind::Packet ||
                    (control && control->controlKind() == MediaControlBufferKind::EndGeneration))
                    cancelPendingOutputTransfer();
            }
            m_packets.clear();
            m_preparedPendingPacket.reset();
            m_generationEnded = false;
            std::optional<std::uint64_t> continuousCutoff;
            if (m_preparedSource) {
                auto cutoff = m_preparedSource->purgeMedia(m_config.streamKind, *purge);
                if (!cutoff) status = ::media::Status::failure(cutoff.error());
                else if (!cutoff.value()) return processWaiting();
                else continuousCutoff = cutoff.value();
            } else discardUnassembledPackets();
            // Retain the already planned batch lease for ordered RTCP processing.
            // Its remaining RTP entries can no longer produce media after this fence.
            m_runtimeBatchMediaRetired = m_runtimeIngressBatch.has_value();
            if (status && m_preparedReceiver && !m_preparedSource)
                status = m_preparedReceiver->discardReplayMedia();
            if (status) {
                if (!m_preparedSource) m_protocol->requireKeyframe();
                status = m_syncGroup->rtpIngressFrontiers()->publish(
                    m_ingressBinding->stream, *purge,
                    m_preparedSource ? *continuousCutoff : m_nextIngressSequence - 1);
            }
            if (status) {
                mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::RuntimeNode,
                    "rtp_ingress_purge node=" + std::to_string(nodeId().value) +
                    " transition=" + std::to_string(purge->transitionSequence) +
                    " cutoff=" + std::to_string(m_nextIngressSequence - 1));
            }
            const auto completed = m_ownerPurge->complete(*purge, status);
            return processProgress(completed ? status : completed);
        }
    }
    return FFmpegNodeRuntime::process(context);
}

std::string_view RawRtpInputNode::generationPurgeIdentity() const noexcept
{
    if (!m_ingressBinding) return {};
    return m_ingressBinding->stream == MediaStreamKind::Video
        ? MediaRtpVideoIngressGenerationIdentity : MediaRtpAudioIngressGenerationIdentity;
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
RawRtpInputNode::generationPurgeTarget() const noexcept
{
    return m_ownerPurge;
}

::media::Result<MediaNodeProcessResult> RawRtpInputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_initialized) {
        return processProgress(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "RawRtpInputNode process requires receiver readiness from start")));
    }
    if (!m_formatEmitted) {
        m_formatEmitted = true;
        return processProgress(emitOutput(context, "format", m_streamSnapshot));
    }
    if (m_preparedSource) {
        if (auto status = m_preparedReceiver->captureStatus(); !status) return processProgress(status);
        if (m_generationEnded) return processWaiting();
        if (!m_preparedPendingPacket) {
            auto output = m_preparedSource->takeOutput(m_config.streamKind, MediaPreparedRtpOutputClass::Packet);
            if (!output) return processProgress(::media::Status::failure(output.error()));
            if (!output.value()) return processWaiting();
            const auto* port = mediaPreparedRtpOutputPortName(output.value()->port);
            if (!port) return processProgress(::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("Prepared RTP output has an unknown protocol port")));
            if (output.value()->port != MediaPreparedRtpOutputPort::Packet)
                return processProgress(::media::Status::failure(::media::ErrorInfo::internalError(
                    "Prepared RTP packet endpoint returned control output")));
            m_preparedPendingPacket = std::move(output.value()->buffer);
        }
        auto generationEnd = mediaEndGeneration(m_preparedPendingPacket);
        if (!generationEnd) return processProgress(::media::Status::failure(generationEnd.error()));
        if (generationEnd.value()) {
            m_generationEnded = true;
            auto output = std::move(m_preparedPendingPacket);
            return processProgress(emitOutput(context, "packet", output));
        }
        const auto bytes = m_preparedPendingPacket->payloadFootprintBytes();
        if (!bytes) return processProgress(::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("Prepared RTP output lacks payload footprint")));
        auto reservation = context.reservePayloadBatch(nodeId(), m_config.streamKind,
            MediaPayloadKind::Packet, std::span<const std::uint64_t>(&*bytes, 1));
        if (!reservation) return processProgress(::media::Status::failure(reservation.error()));
        if (auto status = reservation.value().front().attachTo(*m_preparedPendingPacket); !status)
            return processProgress(status);
        auto output = std::move(m_preparedPendingPacket);
        return processProgress(emitOutput(context, "packet", output));
    }
    if (!m_events.empty()) {
        auto event = std::move(m_events.front());
        m_events.pop_front();
        return processProgress(emitOutput(context, event.first, event.second));
    }
    if (!m_packets.empty()) {
        MediaBufferRef packet = std::move(m_packets.front());
        m_packets.pop_front();
        return processProgress(emitOutput(context, "packet", packet));
    }
    if (m_protocol->hasPending()) {
        if (auto status = drainPendingRtpItems(context); !status) {
            return processProgress(status);
        }
        if (!m_events.empty()) {
            auto event = std::move(m_events.front());
            m_events.pop_front();
            return processProgress(emitOutput(context, event.first, event.second));
        }
        if (!m_packets.empty()) {
            MediaBufferRef packet = std::move(m_packets.front());
            m_packets.pop_front();
            return processProgress(emitOutput(context, "packet", packet));
        }
    }
    const auto now = std::chrono::steady_clock::now();
    const std::int64_t nowNs = mediaSteadyClockNowNs();
    if (auto status = queueClockTransition(context, nowNs); !status) {
        return processProgress(status);
    }
    if (!m_events.empty()) {
        auto event = std::move(m_events.front());
        m_events.pop_front();
        return processProgress(emitOutput(context, event.first, event.second));
    }
    if (auto status = m_protocol->expire(now); !status) return processProgress(status);
    if (auto status = drainPendingRtpItems(context); !status) return processProgress(status);
    if (!m_events.empty()) {
        auto event = std::move(m_events.front());
        m_events.pop_front();
        return processProgress(emitOutput(context, event.first, event.second));
    }
    if (!m_packets.empty()) {
        MediaBufferRef packet = std::move(m_packets.front());
        m_packets.pop_front();
        return processProgress(emitOutput(context, "packet", packet));
    }

    auto timeout = m_protocol->receiveTimeoutMs(now, nowNs, m_cancellableReadTimeoutMs);
    if (!timeout) return processProgress(::media::Status::failure(timeout.error()));
    const int receiveTimeoutMs = timeout.value();
    ::media::Result<MediaRtpUdpDatagram> datagram =
        ::media::Result<MediaRtpUdpDatagram>::failure(
            ::media::ErrorInfo::notInitialized(
                "raw RTP receiver is unavailable"));
    std::int64_t observedAtNs = 0;
    MediaPreparedRtpDatagramOwner preparedStorage;
    if (m_preparedReceiver &&
        m_preparedReceiver->preparedReplayDrained()) {
        if (!m_runtimeIngressBatch) {
            auto receivedBatch = m_preparedReceiver->receiveRuntimeBatch(
                receiveTimeoutMs);
            if (!receivedBatch) {
                datagram = ::media::Result<MediaRtpUdpDatagram>::failure(
                    receivedBatch.error());
            } else {
                m_runtimeIngressBatch.emplace(
                    std::move(receivedBatch).value());
                m_runtimeIngressBatchIndex = 0;
                m_runtimeBatchMediaRetired = false;
                const auto entries = m_runtimeIngressBatch->entries();
                std::size_t batchBytes = 0;
                for (const auto& entry : entries) {
                    batchBytes += entry.bytes.size();
                }
                ++m_runtimeIngressBatches;
                m_runtimeIngressDatagrams += entries.size();
                m_runtimeIngressBytes += batchBytes;
                m_runtimeIngressMaximumBatchDatagrams = (std::max)(
                    m_runtimeIngressMaximumBatchDatagrams, entries.size());
                m_runtimeIngressMaximumBatchBytes = (std::max)(
                    m_runtimeIngressMaximumBatchBytes, batchBytes);
            }
        }
        if (m_runtimeIngressBatch) {
            const auto entries = m_runtimeIngressBatch->entries();
            while (m_runtimeIngressBatchIndex < entries.size() &&
                   m_events.empty() && m_packets.empty()) {
                const auto& entry = entries[m_runtimeIngressBatchIndex++];
                if (m_runtimeBatchMediaRetired && entry.channel == MediaRtpUdpChannel::Rtp)
                    continue;
                const auto bytes = std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(entry.bytes.data()),
                    entry.bytes.size());
                auto status = entry.channel == MediaRtpUdpChannel::Rtp
                    ? processRtp(context, bytes,
                                 entry.observedAtNanoseconds)
                    : processRtcp(context, bytes,
                                  entry.observedAtNanoseconds);
                if (!status) return processProgress(status);
            }
            if (m_runtimeIngressBatchIndex == entries.size()) {
                m_runtimeIngressBatch.reset();
                m_runtimeIngressBatchIndex = 0;
                m_runtimeBatchMediaRetired = false;
            }
            if (!m_events.empty()) {
                auto event = std::move(m_events.front());
                m_events.pop_front();
                return processProgress(emitOutput(
                    context, event.first, event.second));
            }
            if (!m_packets.empty()) {
                MediaBufferRef packet = std::move(m_packets.front());
                m_packets.pop_front();
                return processProgress(emitOutput(
                    context, "packet", packet));
            }
            return processProgress();
        }
    } else if (m_preparedReceiver) {
        auto prepared = m_preparedReceiver->receive(receiveTimeoutMs);
        if (prepared) {
            observedAtNs = prepared.value().observedAtNs;
            preparedStorage = std::move(prepared).value().storage;
        } else {
            datagram = ::media::Result<MediaRtpUdpDatagram>::failure(
                prepared.error());
        }
    } else {
        datagram = m_transport.receive(receiveTimeoutMs);
        if (datagram) observedAtNs = mediaSteadyClockNowNs();
    }
    if (!datagram && !preparedStorage) {
        if (datagram.error().code == ::media::ErrorCode::WouldBlock) {
            if (auto clockStatus = queueClockTransition(
                    context, mediaSteadyClockNowNs()); !clockStatus) {
                return processProgress(clockStatus);
            }
            if (auto status = m_protocol->expire(std::chrono::steady_clock::now()); !status)
                return processProgress(status);
            if (auto status = drainPendingRtpItems(context); !status) return processProgress(status);
            if (!m_events.empty()) {
                auto event = std::move(m_events.front());
                m_events.pop_front();
                return processProgress(emitOutput(context, event.first, event.second));
            }
            if (!m_packets.empty()) {
                MediaBufferRef packet = std::move(m_packets.front());
                m_packets.pop_front();
                return processProgress(emitOutput(context, "packet", packet));
            }
            return processProgress();
        }
        return processProgress(::media::Status::failure(datagram.error()));
    }
    const auto& receivedDatagram = preparedStorage ? preparedStorage->datagram : datagram.value();
    const MediaRtpUdpChannel channel = receivedDatagram.channel;
    const auto bytes = std::span<const std::uint8_t>(
        receivedDatagram.bytes.data(), receivedDatagram.bytes.size());
    auto status = channel == MediaRtpUdpChannel::Rtp
        ? processRtp(context, bytes, observedAtNs)
        : processRtcp(context, bytes, observedAtNs);
    if (!status) return processProgress(status);
    if (!m_events.empty()) {
        auto event = std::move(m_events.front());
        m_events.pop_front();
        return processProgress(emitOutput(context, event.first, event.second));
    }
    if (m_packets.empty()) return processProgress();
    MediaBufferRef packet = std::move(m_packets.front());
    m_packets.pop_front();
    return processProgress(emitOutput(context, "packet", packet));
}

::media::Status RawRtpInputNode::prepareReceiver(MediaGraphExecutionContext& context)
{
    const MediaNodeOptions* options = nodeOptions(context);
    auto family = requiredNodeOption(options, "RawRtpInputNode", "rtp.address_family");
    auto address = requiredNodeOption(options, "RawRtpInputNode", "rtp.bind_address");
    auto rtpPort = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.port");
    auto rtcpPort = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtcp.port");
    auto payloadType = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.payload_type");
    auto clockRate = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.clock_rate");
    auto receiveBuffer = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.receive_buffer_bytes");
    auto datagramBytes = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.maximum_datagram_bytes");
    auto reorderWindow = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.reorder_window_packets");
    auto reorderDelay = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.maximum_reorder_delay_ms");
    auto readTimeout = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtp.cancellable_read_timeout_ms");
    auto requireSr = requiredBoolNodeOption(options, "RawRtpInputNode", "rtcp.require_sender_reports");
    auto requireCname = requiredBoolNodeOption(options, "RawRtpInputNode", "rtcp.require_cname");
    auto srTimeout = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtcp.sender_report_timeout_ms");
    auto cnameTimeout = requiredPositiveIntNodeOption(options, "RawRtpInputNode", "rtcp.cname_timeout_ms");
    auto clockLossPolicy = requiredNodeOption(
        options, "RawRtpInputNode", "rtcp.clock_loss_policy");
    auto maximumExtrapolation = requiredPositiveInt64NodeOption(
        options, "RawRtpInputNode", "rtcp.maximum_extrapolation_ns");
    auto compositionMode = requiredNodeOption(options, "RawRtpInputNode", "rtcp.composition_mode");
    auto streamKind = requiredStreamKindNodeOption(options, "RawRtpInputNode", "rtp.stream_kind");
    auto codec = requiredNodeOption(options, "RawRtpInputNode", "rtp.codec");
    auto fmtp = requiredPossiblyEmptyNodeOption(options, "RawRtpInputNode", "rtp.fmtp");
    auto channels = requiredNonNegativeIntNodeOption(options, "RawRtpInputNode", "rtp.channels");
    auto accessUnitDuration = requiredNonNegativeIntNodeOption(
        options, "RawRtpInputNode", "rtp.access_unit_duration_ticks");
    auto maximumAccessUnitBytes = requiredPositiveInt64NodeOption(
        options, "RawRtpInputNode", "rtp.maximum_access_unit_bytes");
    auto maximumAccessUnitsPerPush = requiredPositiveInt64NodeOption(
        options, "RawRtpInputNode", "rtp.maximum_access_units_per_push");
    auto accessUnitSizeAuthority = requiredNodeOption(
        options, "RawRtpInputNode", "rtp.access_unit_size_authority");
    auto accessUnitCompletionAuthority = requiredNodeOption(
        options, "RawRtpInputNode", "rtp.access_unit_completion_authority");
    if (!family || !address || !rtpPort || !rtcpPort || !payloadType || !clockRate || !receiveBuffer ||
        !datagramBytes || !reorderWindow || !reorderDelay || !readTimeout || !requireSr || !requireCname ||
        !srTimeout || !cnameTimeout || !clockLossPolicy || !maximumExtrapolation || !compositionMode || !streamKind || !codec || !fmtp || !channels || !accessUnitDuration ||
        !maximumAccessUnitBytes || !maximumAccessUnitsPerPush ||
        !accessUnitSizeAuthority || !accessUnitCompletionAuthority) {
        const ::media::ErrorInfo* error = nullptr;
        if (!family) error = &family.error(); else if (!address) error = &address.error(); else if (!rtpPort) error = &rtpPort.error();
        else if (!rtcpPort) error = &rtcpPort.error(); else if (!payloadType) error = &payloadType.error(); else if (!clockRate) error = &clockRate.error();
        else if (!receiveBuffer) error = &receiveBuffer.error(); else if (!datagramBytes) error = &datagramBytes.error(); else if (!reorderWindow) error = &reorderWindow.error();
        else if (!reorderDelay) error = &reorderDelay.error(); else if (!readTimeout) error = &readTimeout.error(); else if (!requireSr) error = &requireSr.error();
        else if (!requireCname) error = &requireCname.error(); else if (!srTimeout) error = &srTimeout.error(); else if (!cnameTimeout) error = &cnameTimeout.error();
        else if (!clockLossPolicy) error = &clockLossPolicy.error();
        else if (!maximumExtrapolation) error = &maximumExtrapolation.error();
        else if (!compositionMode) error = &compositionMode.error(); else if (!streamKind) error = &streamKind.error(); else if (!codec) error = &codec.error();
        else if (!fmtp) error = &fmtp.error();
        else if (!channels) error = &channels.error();
        else if (!accessUnitDuration) error = &accessUnitDuration.error();
        else if (!maximumAccessUnitBytes) error = &maximumAccessUnitBytes.error();
        else if (!maximumAccessUnitsPerPush) error = &maximumAccessUnitsPerPush.error();
        else if (!accessUnitSizeAuthority) error = &accessUnitSizeAuthority.error();
        else error = &accessUnitCompletionAuthority.error();
        return ::media::Status::failure(*error);
    }
    auto rtcpComposition = parseMediaRtcpCompositionMode(compositionMode.value());
    if (!rtcpComposition) return ::media::Status::failure(rtcpComposition.error());
    MediaRtpClockLossPolicy lossPolicy;
    if (clockLossPolicy.value() == "fail_on_degraded") {
        lossPolicy = MediaRtpClockLossPolicy::FailOnDegraded;
    } else if (clockLossPolicy.value() == "fail_on_expired") {
        lossPolicy = MediaRtpClockLossPolicy::FailOnExpired;
    } else if (clockLossPolicy.value() == "wait_for_evidence") {
        lossPolicy = MediaRtpClockLossPolicy::WaitForEvidence;
    } else if (clockLossPolicy.value() == "invalidate_and_wait") {
        lossPolicy = MediaRtpClockLossPolicy::InvalidateAndWait;
    } else {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RawRtpInputNode clock loss policy is invalid"));
    }
    if (rtpPort.value() > 65535 || rtcpPort.value() > 65535 || payloadType.value() > 127 ||
        (family.value() != "ipv4" && family.value() != "ipv6")) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RawRtpInputNode planned numeric option is out of range"));
    }
    auto waitForKeyFrame = requiredBoolNodeOption(
        options, "RawRtpInputNode", "rtp.wait_for_keyframe_after_loss");
    if (!waitForKeyFrame) return ::media::Status::failure(waitForKeyFrame.error());
    m_config = MediaRtpDepacketizerConfig{
        streamKind.value(), codec.value(), fmtp.value(),
        static_cast<uint8_t>(payloadType.value()), clockRate.value(), channels.value(),
        accessUnitDuration.value(), waitForKeyFrame.value()};
    m_accessUnitEnvelope = MediaPreparedRtpAccessUnitEnvelope{
        m_config.streamKind, m_config.codecName,
        static_cast<std::uint64_t>(maximumAccessUnitBytes.value()),
        static_cast<std::uint64_t>(maximumAccessUnitsPerPush.value()),
        accessUnitSizeAuthority.value(),
        accessUnitCompletionAuthority.value()};
    if (auto status = m_accessUnitEnvelope.validate(); !status) {
        return status;
    }
    auto depacketizer = MediaRtpDepacketizerFactory::create(
        m_config, m_accessUnitEnvelope.maximumAccessUnitBytes, MediaRuntimeLineageAllocation{});
    if (!depacketizer) return ::media::Status::failure(depacketizer.error());
    auto snapshot = MediaRawRtpStreamDescriptorFactory::create(m_config);
    if (!snapshot) return ::media::Status::failure(snapshot.error());
    std::optional<MediaRtpIngressPlan> preparedIngressPlan;
    if (m_requiresPreparedInput) {
        auto ingressPlan = MediaRtpIngressNodePlanDecoder::decode(options);
        if (!ingressPlan) {
            return ::media::Status::failure(ingressPlan.error());
        }
        if (ingressPlan.value().maximumDatagramBytes() !=
                static_cast<std::size_t>(datagramBytes.value()) ||
            ingressPlan.value().socketReceiveCapacityBytes() !=
                static_cast<std::size_t>(receiveBuffer.value()) ||
            ingressPlan.value().reorderWindowPackets() !=
                static_cast<std::size_t>(reorderWindow.value())) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RawRtpInputNode transport options differ from its ingress product"));
        }
        if (!m_prepared.valid() || !m_prepared.kind() ||
            *m_prepared.kind() != MediaPreparedRealtimeInputKind::RawRtp) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "RawRtpInputNode requires its exact prepared raw RTP binding"));
        }
        auto released = m_prepared.releaseBuffer();
        if (!released) return ::media::Status::failure(released.error());
        auto buffer = std::dynamic_pointer_cast<
            MediaRawRtpPreparedInputBuffer>(released.value());
        if (!buffer || buffer->type() !=
                MediaBufferType::RawRtpPreparedInput) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RawRtpInputNode prepared binding has wrong buffer type"));
        }
        if (auto status = buffer->validateRuntimeIngressPlan(
                ingressPlan.value()); !status) {
            return status;
        }
        preparedIngressPlan.emplace(std::move(ingressPlan).value());
        if (m_preparedSource) {
            if (m_config.streamKind != MediaStreamKind::Video &&
                (m_config.streamKind != MediaStreamKind::Audio || !m_preparedSource->plan().audio))
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Prepared RTP source does not own this runtime stream"));
            const auto& installed = m_config.streamKind == MediaStreamKind::Video
                ? m_preparedSource->plan().video : *m_preparedSource->plan().audio;
            const auto depay = [](const auto& c) { return std::tuple(c.streamKind, c.codecName,
                c.fmtp, c.payloadType, c.clockRate, c.channels, c.accessUnitDurationRtpTicks, c.waitForKeyFrameAfterLoss); };
            const auto envelope = [](const auto& e) { return std::tuple(e.streamKind, e.codecName,
                e.maximumAccessUnitBytes, e.maximumAccessUnitsPerPush, e.sizeAuthority, e.completionAuthority); };
            if (depay(installed.depacketizer) != depay(m_config) || envelope(installed.envelope) != envelope(m_accessUnitEnvelope) ||
                installed.reorder.windowPackets != preparedIngressPlan->reorderWindowPackets() ||
                installed.reorder.maximumDelay.count() != preparedIngressPlan->maximumReorderDelayNanoseconds() ||
                installed.reorder.payloadType != m_config.payloadType ||
                installed.workingSet.maximumDatagramBytes != preparedIngressPlan->maximumDatagramBytes() ||
                installed.clock.tracker.requireSenderReports != requireSr.value() ||
                installed.clock.tracker.requireCname != requireCname.value() ||
                installed.clock.tracker.senderReportTimeoutNs != static_cast<std::int64_t>(srTimeout.value()) * 1'000'000 ||
                installed.clock.tracker.cnameTimeoutNs != static_cast<std::int64_t>(cnameTimeout.value()) * 1'000'000 ||
                installed.clock.maximumExtrapolationNs != maximumExtrapolation.value() ||
                installed.clock.lossPolicy != lossPolicy || installed.clock.compositionMode != rtcpComposition.value())
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument("Prepared RTP protocol differs from runtime transport policy"));
            if (auto status = buffer->beginContinuousRuntime(m_preparedSource); !status) return status;
            m_preparedReceiver = std::move(buffer);
            m_streamSnapshot = MediaBufferRef(std::move(snapshot).value());
            m_cancellableReadTimeoutMs = readTimeout.value();
            m_initialized = true;
            return ::media::Status::success();
        }
        auto replay = buffer->beginReplay();
        if (!replay) return ::media::Status::failure(replay.error());
        if (buffer->streamKind() != m_config.streamKind ||
            replay.value().identity.streamKind != m_config.streamKind ||
            replay.value().identity.payloadType != m_config.payloadType ||
            replay.value().identity.clockRate != m_config.clockRate ||
            replay.value().identity.codecName != m_config.codecName) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RawRtpInputNode prepared transport identity conflicts with plan"));
        }
        if (m_config.streamKind == MediaStreamKind::Video) {
            if (!replay.value().videoSignaling) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "prepared RTP video input requires detected signaling"));
            }
            const auto& signaling = *replay.value().videoSignaling;
            mediaGraphDiagnosticLog(
                MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "rtp_fmtp_probe mode=auto codec=" + m_config.codecName +
                    " payload_type=" + std::to_string(m_config.payloadType) +
                    " parameter_sets=" +
                    (m_config.codecName == "h264" ? "sps,pps" : "vps,sps,pps") +
                    " packets=" + std::to_string(signaling.packetCount) +
                    " bytes=" + std::to_string(signaling.datagramBytes) +
                    " elapsed_ms=" +
                    std::to_string(signaling.elapsedMilliseconds));
        } else if (replay.value().videoSignaling) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "prepared RTP audio input rejects video signaling"));
        }
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_prepared_input stream=" +
                std::string(m_config.streamKind == MediaStreamKind::Video
                    ? "video" : "audio") +
                " rtp_datagrams=" +
                std::to_string(replay.value().rtpDatagrams) +
                " rtcp_datagrams=" +
                std::to_string(replay.value().rtcpDatagrams) +
                " arrival_span_ms=" +
                std::to_string(replay.value().arrivalSpanMilliseconds) +
                " replay_offset_ms=" +
                std::to_string(replay.value().replayOffsetMilliseconds));
        m_preparedReceiver = std::move(buffer);
    } else {
        auto transport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
            family.value() == "ipv6" ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
            address.value(), static_cast<uint16_t>(rtpPort.value()), static_cast<uint16_t>(rtcpPort.value()),
            receiveBuffer.value(), static_cast<std::size_t>(datagramBytes.value()), readTimeout.value(),
            nullptr}, *std::pmr::new_delete_resource());
        if (!transport) return ::media::Status::failure(transport.error());
        m_transport = std::move(transport).value();
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_fmtp mode=manual codec=" + m_config.codecName +
                " payload_type=" + std::to_string(m_config.payloadType));
    }
    const MediaRtpReorderConfig reorderConfig{
        static_cast<std::size_t>(reorderWindow.value()),
        preparedIngressPlan
            ? std::chrono::nanoseconds(
                  preparedIngressPlan->maximumReorderDelayNanoseconds())
            : std::chrono::duration_cast<std::chrono::nanoseconds>(
                  std::chrono::milliseconds(reorderDelay.value())),
        static_cast<uint8_t>(payloadType.value())};
    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "rtp_reorder window_packets=" + std::to_string(reorderConfig.windowPackets) +
        " maximum_delay_ns=" + std::to_string(reorderConfig.maximumDelay.count()));
    auto protocol = MediaRtpInputProtocolSession::create(m_config, m_accessUnitEnvelope, reorderConfig, {
        {requireSr.value(), requireCname.value(),
         static_cast<std::int64_t>(srTimeout.value()) * 1'000'000,
         static_cast<std::int64_t>(cnameTimeout.value()) * 1'000'000},
        maximumExtrapolation.value(), lossPolicy, rtcpComposition.value()},
        std::move(depacketizer).value(), MediaRuntimeLineageAllocation{});
    if (!protocol) return ::media::Status::failure(protocol.error());
    m_protocol = std::make_unique<MediaRtpInputProtocolSession>(std::move(protocol).value());
    m_streamSnapshot = MediaBufferRef(std::move(snapshot).value());
    m_cancellableReadTimeoutMs = readTimeout.value();
    m_initialized = true;
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::processRtp(
    MediaGraphExecutionContext& context,
    std::span<const std::uint8_t> datagram,
    std::int64_t observedAtNs)
{
    if (observedAtNs <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RawRtpInputNode requires a monotonic RTP arrival time"));
    }
    auto parsed = MediaRtpPacketParser::parse(datagram);
    if (!parsed) return ::media::Status::failure(parsed.error());
    if (parsed.value().payloadType == m_config.payloadType) {
        const auto activity = context.inputActivity();
        if (!activity) {
            return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                "RTP input requires runtime activity evidence"));
        }
        activity->observe(observedAtNs);
    }
    if (auto status = m_protocol->observeRtp(std::move(parsed).value(), observedAtNs); !status)
        return status;
    return drainPendingRtpItems(context);
}

::media::Status RawRtpInputNode::drainPendingRtpItems(
    MediaGraphExecutionContext& context)
{
    while (m_protocol->hasPending()) {
        auto& item = m_protocol->pendingFront();
        if (const auto* packet = std::get_if<MediaRtpPacket>(&item)) {
            if (auto status = queueClockEvidence(context, mediaSteadyClockNowNs()); !status) {
                return status;
            }
            // Publish ordered evidence before payload allocation can wait for credits.
            if (!m_events.empty()) return ::media::Status::success();
            if (auto status = processPendingRtpPacket(context, *packet); !status) {
                return status;
            }
            m_protocol->consumeFront();
            continue;
        }
        const auto& [discontinuity, generationBeforeObservation] =
            std::get<std::pair<MediaRtpDiscontinuity, std::uint64_t>>(item);
        const auto* sourceOptions = nodeOptions(context);
        if (sourceOptions && sourceOptions->has("source.playback_epoch.identity_transition")) {
            if (sourceOptions->value("source.playback_epoch.identity_transition") != "replan_session") {
                return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
                    "raw RTP input has an unsupported playback source identity transition policy"));
            }
            if (discontinuity.reason == MediaRtpDiscontinuityReason::SsrcChanged ||
                discontinuity.reason == MediaRtpDiscontinuityReason::PayloadTypeChanged) {
                return ::media::Status::failure(::media::ErrorInfo::unsupported(
                    "RTP source identity changed; shared playback session requires replanning"));
            }
        }
        auto prepared = m_protocol->prepareDiscontinuity(generationBeforeObservation);
        if (!prepared) return ::media::Status::failure(prepared.error());
        if (auto status = publishClockObservation(context, std::move(prepared).value(), mediaSteadyClockNowNs(), &discontinuity); !status) return status;
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_discontinuity stream=" +
                std::string(m_config.streamKind == MediaStreamKind::Video
                    ? "video" : "audio") +
                " reason=" + discontinuityReasonName(discontinuity.reason) +
                " first_missing=" +
                std::to_string(discontinuity.firstMissingSequence) +
                " resumed=" +
                std::to_string(discontinuity.resumedSequence) +
                " generation_before=" +
                std::to_string(generationBeforeObservation) +
                " generation_after=" +
                std::to_string(m_protocol->generation()));
        m_protocol->consumeFront();
    }
    return queueClockEvidence(context, mediaSteadyClockNowNs());
}

::media::Status RawRtpInputNode::processPendingRtpPacket(
    MediaGraphExecutionContext& context,
    const MediaRtpPacket& packet)
{
    auto action = m_protocol->preparePacket(packet, m_pendingPayloadReservations.size());
    if (!action) {
        m_pendingPayloadReservations.clear();
        m_protocol->releasePayloadState();
        return ::media::Status::failure(action.error());
    }
    if (const auto* rejected = std::get_if<MediaRtpPacketRejection>(&action.value())) {
        auto failure = m_protocol->commitPacketRejection(*rejected);
        m_pendingPayloadReservations.clear();
        return failure;
    }
    const auto& allocation = std::get<MediaRtpPacketAllocationAction>(action.value());
    if (allocation.drop) return ::media::Status::success();
    if (allocation.releaseExisting) {
        m_pendingPayloadReservations.clear();
        m_protocol->releasePayloadState();
    }
    if (m_pendingPayloadReservations.empty()) {
        auto reservations = context.reservePayloadBatch(
            nodeId(), m_config.streamKind, MediaPayloadKind::Packet, allocation.reserveBytes);
        if (!reservations) {
            m_protocol->releasePayloadState();
            return ::media::Status::failure(reservations.error());
        }
        m_pendingPayloadReservations = std::move(reservations).value();
    }
    auto completion = m_protocol->commitPacket(packet, m_pendingPayloadReservations.size());
    if (!completion) {
        m_pendingPayloadReservations.clear();
        m_protocol->releasePayloadState();
        return ::media::Status::failure(completion.error());
    }
    auto& accessUnits = completion.value().media.accessUnits;
    if (!accessUnits.empty()) {
        for (std::size_t index = 0; index < accessUnits.size(); ++index) {
            MediaRtpAccessUnit& unit = accessUnits[index];
            const bool depacketizedKey =
                (unit.packet->flags & AV_PKT_FLAG_KEY) != 0;
            // GStreamer wait-for-keyframe semantics: after loss, admit only a
            // complete random-access AU before resuming dependent pictures.
            const auto admission = m_protocol->admitAccessUnit(depacketizedKey);
            if (!admission.accepted) continue;
            if (admission.recoveredKeyframe) {
                mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::RuntimeNode,
                    "rtp_recovery state=complete_keyframe_received");
            }
            const auto actualBytes =
                static_cast<std::uint64_t>(unit.packet->size);
            auto buffer = FFmpegBufferFactory::wrapPacket(*std::pmr::new_delete_resource(),
                std::move(unit.packet), m_config.streamKind, std::nullopt, std::nullopt,
                MediaRtpPacketProvenance{packet.ssrc, m_protocol->generation(),
                                         nextIngressSequence()});
            if (!buffer) return ::media::Status::failure(buffer.error());
            if (auto status = m_pendingPayloadReservations[index]
                    .shrinkToActual(actualBytes); !status) {
                return status;
            }
            if (auto status = m_pendingPayloadReservations[index]
                    .attachTo(*buffer.value()); !status) {
                return status;
            }
            if (m_config.streamKind == MediaStreamKind::Video &&
                depacketizedKey && !m_keyTraceEmitted) {
                m_keyTraceEmitted = true;
                mediaGraphDiagnosticLog(
                    MediaGraphDiagnosticLevel::State,
                    MediaGraphDiagnosticPhase::RuntimeNode,
                    std::string("rtp_key_trace stage=raw_rtp depacketized=") +
                        (depacketizedKey ? "1" : "0") +
                        " wrapped=" +
                        (buffer.value()->isKeyFrame() ? "1" : "0"));
            }
            buffer.value()->setTimeDescriptor(MediaTimeDescriptor{unit.timeBase});
            MediaFormatDescriptor format;
            format.streamKind = m_config.streamKind;
            format.streamIndex = 0;
            format.time.timeBase = unit.timeBase;
            format.isInput = true;
            format.isRealtime = true;
            buffer.value()->setFormatDescriptor(std::move(format));
            m_packets.push_back(std::move(buffer).value());
        }
        m_pendingPayloadReservations.clear();
    } else if (completion.value().releaseReservations) {
        m_pendingPayloadReservations.clear();
    }
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::processRtcp(
    MediaGraphExecutionContext& context,
    std::span<const std::uint8_t> datagram,
    std::int64_t observedAtNs)
{
    if (observedAtNs <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "RawRtpInputNode requires a monotonic RTCP arrival time"));
    }
    const auto evaluationAtNs = mediaSteadyClockNowNs();
    auto observation = m_protocol->prepareRtcp(datagram, observedAtNs, evaluationAtNs,
        MediaRtpByeHandling::InvalidateMember);
    if (!observation) return ::media::Status::failure(observation.error());
    return publishClockObservation(context, std::move(observation).value(), evaluationAtNs, nullptr);
}

::media::Status RawRtpInputNode::publishClockObservation(MediaGraphExecutionContext& context,
    MediaRtpPreparedClockObservation prepared, std::int64_t evaluationAtNs, const MediaRtpDiscontinuity* discontinuity)
{
    auto rejection = std::move(prepared.tracker.rejection);
    const bool invalidated = prepared.tracker.invalidated || prepared.retireAssembly || discontinuity;
    const bool selectedClock = std::holds_alternative<MediaRtcpClockEvidence>(prepared.tracker.output);
    std::optional<RtpEvidenceDiagnostic> diagnostic;
    if (const auto* evidence = std::get_if<MediaRtcpClockEvidence>(&prepared.tracker.output))
        diagnostic.emplace(evidence->generation, evidence->observedMediaSsrc, evidence->senderReportObservedAtNs);
    const char* port = selectedClock ? "clock" : "event";
    const bool queued = (discontinuity || !std::holds_alternative<std::monostate>(prepared.tracker.output)) &&
        context.findOutputChannel(nodeId(), port);
    if (queued) {
        if (m_nextIngressSequence == 0 || m_nextIngressSequence == std::numeric_limits<std::uint64_t>::max())
            return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RTP ingress sequence exhausted"));
        MediaBufferRef output;
        if (discontinuity)
            output = makeMediaBufferRef<MediaRtpIngressEventBuffer>(*discontinuity,
                prepared.tracker.state.generation, m_nextIngressSequence);
        else if (auto* value = std::get_if<MediaRtcpClockEvidence>(&prepared.tracker.output))
            output = makeMediaBufferRef<MediaRtpIngressEventBuffer>(std::move(*value), m_nextIngressSequence);
        else
            output = makeMediaBufferRef<MediaRtpIngressEventBuffer>(
                std::get<MediaRtpClockInvalidation>(prepared.tracker.output), m_nextIngressSequence);
        m_events.emplace_back(port, std::move(output));
    }
    auto committed = discontinuity ? m_protocol->commitDiscontinuity(discontinuity->reason, std::move(prepared))
        : m_protocol->commitClockObservation(std::move(prepared));
    if (!committed) {
        if (queued) m_events.pop_back();
        return committed;
    }
    if (invalidated) m_pendingPayloadReservations.clear();
    if (rejection) return ::media::Status::failure(std::move(*rejection));
    if (queued) ++m_nextIngressSequence;
    if (diagnostic) logRtpClockEvidence(m_config.streamKind, *diagnostic, evaluationAtNs);
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::queueClockEvidence(MediaGraphExecutionContext& context, std::int64_t now)
{
    auto prepared = m_protocol->prepareEvidence(now);
    if (!prepared) return ::media::Status::failure(prepared.error());
    return publishClockObservation(context, std::move(prepared).value(), now, nullptr);
}

::media::Status RawRtpInputNode::queueClockTransition(
    MediaGraphExecutionContext& context,
    std::int64_t observedAtNs)
{
    auto transition = m_protocol->prepareClockTransition(observedAtNs);
    if (!transition) return ::media::Status::failure(transition.error());
    const auto ageTransition = transition.value().age;
    if (auto status = publishClockObservation(context, std::move(transition).value(), observedAtNs, nullptr); !status) return status;
    if (!ageTransition) return ::media::Status::success();
    const char* stream = m_config.streamKind == MediaStreamKind::Video
        ? "video"
        : "audio";
    const char* age = *ageTransition == MediaRtpClockAgeTransition::Degraded
        ? "degraded"
        : "expired";
    const auto& clockConfig = m_protocol->clockConfig();
    if (clockConfig.lossPolicy == MediaRtpClockLossPolicy::WaitForEvidence ||
        clockConfig.lossPolicy == MediaRtpClockLossPolicy::InvalidateAndWait) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            std::string("rtp_recovery state=waiting_for_clock_evidence stream=") + stream);
        return ::media::Status::success();
    }
    return ::media::Status::failure(::media::ErrorInfo::ioFailure(
        std::string("RTP ") + stream +
        " source clock evidence " + age +
        "; sender_report_timeout_ns=" +
        std::to_string(clockConfig.tracker.senderReportTimeoutNs) +
        " maximum_extrapolation_ns=" +
        std::to_string(clockConfig.maximumExtrapolationNs) +
        " cname_timeout_ns=" +
        std::to_string(clockConfig.tracker.cnameTimeoutNs)));
}

::media::Status RawRtpInputNode::stop(MediaGraphExecutionContext& context)
{
    auto transportStatus = m_preparedReceiver
        ? m_preparedReceiver->stop()
        : m_transport.stop();
    m_transport.close();
    logIngressBatchTelemetry();
    resetState();
    auto baseStatus = FFmpegNodeRuntime::stop(context);
    return !transportStatus ? transportStatus : baseStatus;
}

void RawRtpInputNode::interrupt(MediaGraphExecutionContext&) noexcept
{
    if (m_preparedReceiver) {
        (void)m_preparedReceiver->interruptReceive();
    } else {
        (void)m_transport.interruptReceive();
    }
}

void RawRtpInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (m_preparedReceiver) {
        m_preparedReceiver->abort();
    } else {
        (void)m_transport.abort();
    }
    m_transport.close();
    logIngressBatchTelemetry();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void RawRtpInputNode::discardUnassembledPackets() noexcept
{
    // Owner-thread source boundary: no old reordered fragment may acquire the
    // tracker generation observed after a BYE or expired clock evidence.
    m_protocol->resetAssembly();
    m_pendingPayloadReservations.clear();
}

void RawRtpInputNode::resetState() noexcept
{
    if (m_ownerPurge) m_ownerPurge->stop();
    m_syncGroup.reset();
    m_protocol.reset();
    m_config = {};
    m_accessUnitEnvelope = {};
    m_pendingPayloadReservations.clear();
    m_streamSnapshot.reset();
    m_packets.clear();
    m_events.clear();
    m_preparedReceiver.reset();
    m_preparedPendingPacket.reset();
    m_generationEnded = false;
    m_runtimeIngressBatch.reset();
    m_runtimeIngressBatchIndex = 0;
    m_runtimeBatchMediaRetired = false;
    m_runtimeIngressBatches = 0;
    m_runtimeIngressDatagrams = 0;
    m_runtimeIngressBytes = 0;
    m_runtimeIngressMaximumBatchDatagrams = 0;
    m_runtimeIngressMaximumBatchBytes = 0;
    m_initialized = false;
    m_formatEmitted = false;
    m_keyTraceEmitted = false;
    m_cancellableReadTimeoutMs = 0;
    m_nextIngressSequence = 1;
}

void RawRtpInputNode::logIngressBatchTelemetry() const
{
    if (m_runtimeIngressBatches == 0) return;
    mediaGraphDiagnosticLog(
        MediaGraphDiagnosticLevel::State,
        MediaGraphDiagnosticPhase::RuntimeNode,
        "rtp_ingress_batch stream=" +
            std::string(m_config.streamKind == MediaStreamKind::Video
                ? "video" : "audio") +
            " batches=" + std::to_string(m_runtimeIngressBatches) +
            " datagrams=" + std::to_string(m_runtimeIngressDatagrams) +
            " bytes=" + std::to_string(m_runtimeIngressBytes) +
            " maximum_batch_datagrams=" +
                std::to_string(m_runtimeIngressMaximumBatchDatagrams) +
            " maximum_batch_bytes=" +
                std::to_string(m_runtimeIngressMaximumBatchBytes));
}

std::uint64_t RawRtpInputNode::nextIngressSequence() noexcept
{
    return m_nextIngressSequence++;
}

} // namespace media::ffmpeg::graph
