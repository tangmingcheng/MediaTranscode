#include "internal/graph/nodes/input/RawRtpInputNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/nodes/input/MediaRawRtpStreamDescriptorFactory.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizerFactory.h"
#include "internal/graph/protocol/rtp/MediaRtpPacketParser.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/buffer/MediaRtpIngressEventBuffer.h"
#include "internal/graph/runtime/buffer/MediaRawRtpPreparedInputBuffer.h"

#include <chrono>
#include <algorithm>
#include <string>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

int64_t steadyNowNs() noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

RawRtpInputNode::RawRtpInputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RawRtpInputNode")
{
}

RawRtpInputNode::RawRtpInputNode(
    MediaNodeId nodeId,
    MediaPreparedRealtimeInput prepared)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RawRtpInputNode"),
      m_prepared(std::move(prepared)),
      m_requiresPreparedInput(true)
{
}

MediaNodeKind RawRtpInputNode::staticKind() noexcept
{
    return MediaNodeKind::RawRtpInput;
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
    if (!status) {
        m_transport.close();
        resetState();
        FFmpegNodeRuntime::abort(context);
    }
    return status;
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
    if (!m_preparedDatagrams.empty()) {
        MediaRtpUdpDatagram datagram = std::move(m_preparedDatagrams.front());
        m_preparedDatagrams.pop_front();
        auto status = datagram.channel == MediaRtpUdpChannel::Rtp
            ? processRtp(context, std::move(datagram))
            : processRtcp(context, std::move(datagram));
        if (!status) return processProgress(status);
        if (m_preparedDatagrams.empty() && !m_preparedQueueTraceEmitted) {
            m_preparedQueueTraceEmitted = true;
            mediaGraphDiagnosticLog(
                MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode,
                "rtp_fmtp_probe prepared_queue_consumed=1");
        }
        if (!m_events.empty()) {
            auto event = std::move(m_events.front());
            m_events.pop_front();
            return processProgress(
                emitOutput(context, event.first, event.second));
        }
        if (!m_packets.empty()) {
            MediaBufferRef packet = std::move(m_packets.front());
            m_packets.pop_front();
            return processProgress(emitOutput(context, "packet", packet));
        }
        return processProgress();
    }

    const auto now = std::chrono::steady_clock::now();
    const std::int64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    if (auto status = queueClockTransition(context, nowNs); !status) {
        return processProgress(status);
    }
    if (!m_events.empty()) {
        auto event = std::move(m_events.front());
        m_events.pop_front();
        return processProgress(emitOutput(context, event.first, event.second));
    }
    auto expired = m_reorder->expire(now);
    if (!expired) return processProgress(::media::Status::failure(expired.error()));
    if (auto status = processReordered(context, std::move(expired).value(),
                                       m_clockTracker->generation()); !status) {
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

    int receiveTimeoutMs = m_cancellableReadTimeoutMs;
    if (const auto deadline = m_reorder->nextDeadline()) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now);
        const int deadlineTimeoutMs = remaining.count() > 0 ? static_cast<int>(remaining.count()) : 1;
        receiveTimeoutMs = std::min(receiveTimeoutMs, deadlineTimeoutMs);
    }
    if (m_clockSchedule) {
        auto clockTimeout = m_clockSchedule->receiveTimeoutMs(nowNs, receiveTimeoutMs);
        if (!clockTimeout) return processProgress(::media::Status::failure(clockTimeout.error()));
        receiveTimeoutMs = clockTimeout.value();
    }
    auto datagram = m_transport.receive(receiveTimeoutMs);
    if (!datagram) {
        if (datagram.error().code == ::media::ErrorCode::WouldBlock) {
            if (auto clockStatus = queueClockTransition(context, steadyNowNs()); !clockStatus) {
                return processProgress(clockStatus);
            }
            auto timedOut = m_reorder->expire(std::chrono::steady_clock::now());
            if (!timedOut) return processProgress(::media::Status::failure(timedOut.error()));
            if (auto status = processReordered(context, std::move(timedOut).value(),
                                               m_clockTracker->generation()); !status) {
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
            return processProgress();
        }
        return processProgress(::media::Status::failure(datagram.error()));
    }
    auto status = datagram.value().channel == MediaRtpUdpChannel::Rtp
        ? processRtp(context, std::move(datagram).value())
        : processRtcp(context, std::move(datagram).value());
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
    const bool sourceClockMappingEnabled = options && options->has("rtcp.maximum_extrapolation_ns");
    ::media::Result<std::int64_t> maximumExtrapolation =
        ::media::Result<std::int64_t>::failure(
            ::media::ErrorInfo::notInitialized("RTP source clock mapping is not enabled"));
    if (sourceClockMappingEnabled) {
        maximumExtrapolation = requiredPositiveInt64NodeOption(
            options, "RawRtpInputNode", "rtcp.maximum_extrapolation_ns");
    }
    auto compositionMode = requiredNodeOption(options, "RawRtpInputNode", "rtcp.composition_mode");
    auto streamKind = requiredStreamKindNodeOption(options, "RawRtpInputNode", "rtp.stream_kind");
    auto codec = requiredNodeOption(options, "RawRtpInputNode", "rtp.codec");
    auto fmtp = requiredPossiblyEmptyNodeOption(options, "RawRtpInputNode", "rtp.fmtp");
    auto channels = requiredNonNegativeIntNodeOption(options, "RawRtpInputNode", "rtp.channels");
    auto accessUnitDuration = requiredNonNegativeIntNodeOption(
        options, "RawRtpInputNode", "rtp.access_unit_duration_ticks");
    if (!family || !address || !rtpPort || !rtcpPort || !payloadType || !clockRate || !receiveBuffer ||
        !datagramBytes || !reorderWindow || !reorderDelay || !readTimeout || !requireSr || !requireCname ||
        !srTimeout || !cnameTimeout || !clockLossPolicy || !compositionMode || !streamKind || !codec || !fmtp || !channels || !accessUnitDuration) {
        const ::media::ErrorInfo* error = nullptr;
        if (!family) error = &family.error(); else if (!address) error = &address.error(); else if (!rtpPort) error = &rtpPort.error();
        else if (!rtcpPort) error = &rtcpPort.error(); else if (!payloadType) error = &payloadType.error(); else if (!clockRate) error = &clockRate.error();
        else if (!receiveBuffer) error = &receiveBuffer.error(); else if (!datagramBytes) error = &datagramBytes.error(); else if (!reorderWindow) error = &reorderWindow.error();
        else if (!reorderDelay) error = &reorderDelay.error(); else if (!readTimeout) error = &readTimeout.error(); else if (!requireSr) error = &requireSr.error();
        else if (!requireCname) error = &requireCname.error(); else if (!srTimeout) error = &srTimeout.error(); else if (!cnameTimeout) error = &cnameTimeout.error();
        else if (!clockLossPolicy) error = &clockLossPolicy.error();
        else if (!compositionMode) error = &compositionMode.error(); else if (!streamKind) error = &streamKind.error(); else if (!codec) error = &codec.error();
        else if (!fmtp) error = &fmtp.error();
        else if (!channels) error = &channels.error(); else error = &accessUnitDuration.error();
        return ::media::Status::failure(*error);
    }
    auto rtcpComposition = parseMediaRtcpCompositionMode(compositionMode.value());
    if (!rtcpComposition) return ::media::Status::failure(rtcpComposition.error());
    MediaRtpClockLossPolicy lossPolicy;
    if (clockLossPolicy.value() == "fail_on_degraded") {
        lossPolicy = MediaRtpClockLossPolicy::FailOnDegraded;
    } else if (clockLossPolicy.value() == "fail_on_expired") {
        lossPolicy = MediaRtpClockLossPolicy::FailOnExpired;
    } else {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "RawRtpInputNode clock loss policy is invalid"));
    }
    if (rtpPort.value() > 65535 || rtcpPort.value() > 65535 || payloadType.value() > 127 ||
        (family.value() != "ipv4" && family.value() != "ipv6")) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument("RawRtpInputNode planned numeric option is out of range"));
    }
    m_config = MediaRtpDepacketizerConfig{
        streamKind.value(), codec.value(), fmtp.value(),
        static_cast<uint8_t>(payloadType.value()), clockRate.value(), channels.value(), accessUnitDuration.value()};
    auto depacketizer = MediaRtpDepacketizerFactory::create(m_config);
    if (!depacketizer) return ::media::Status::failure(depacketizer.error());
    auto snapshot = MediaRawRtpStreamDescriptorFactory::create(m_config);
    if (!snapshot) return ::media::Status::failure(snapshot.error());
    if (m_requiresPreparedInput) {
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
        auto prepared = buffer->takePreparedInput();
        if (!prepared) return ::media::Status::failure(prepared.error());
        if (prepared.value().transport.rtpPort() != rtpPort.value() ||
            prepared.value().transport.rtcpPort() != rtcpPort.value() ||
            prepared.value().signaling.payloadType != m_config.payloadType ||
            prepared.value().signaling.clockRate != m_config.clockRate ||
            prepared.value().signaling.codecName != m_config.codecName) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RawRtpInputNode prepared transport or signaling identity conflicts with plan"));
        }
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_fmtp_probe mode=auto codec=" + m_config.codecName +
                " payload_type=" + std::to_string(m_config.payloadType) +
                " packets=" + std::to_string(
                    prepared.value().signaling.packetCount) +
                " bytes=" + std::to_string(
                    prepared.value().signaling.datagramBytes) +
                " elapsed_ms=" + std::to_string(
                    prepared.value().signaling.elapsedMilliseconds));
        MediaPreparedRawRtpInput payload = std::move(prepared).value();
        m_transport = std::move(payload.transport);
        m_preparedDatagrams = std::move(payload.datagrams);
    } else {
        auto transport = MediaRtpUdpTransport::open(MediaRtpUdpTransportConfig{
            family.value() == "ipv6" ? MediaIpAddressFamily::Ipv6 : MediaIpAddressFamily::Ipv4,
            address.value(), static_cast<uint16_t>(rtpPort.value()), static_cast<uint16_t>(rtcpPort.value()),
            receiveBuffer.value(), static_cast<std::size_t>(datagramBytes.value()), readTimeout.value(),
            nullptr});
        if (!transport) return ::media::Status::failure(transport.error());
        m_transport = std::move(transport).value();
        mediaGraphDiagnosticLog(
            MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::RuntimeNode,
            "rtp_fmtp_probe mode=manual codec=" + m_config.codecName +
                " payload_type=" + std::to_string(m_config.payloadType));
    }
    m_reorder = std::make_unique<MediaRtpReorderBuffer>(MediaRtpReorderConfig{
        static_cast<std::size_t>(reorderWindow.value()), std::chrono::milliseconds(reorderDelay.value()),
        static_cast<uint8_t>(payloadType.value())});
    m_depacketizer = std::move(depacketizer).value();
    m_clockTracker = std::make_unique<MediaRtcpSenderReportTracker>(MediaRtcpSenderReportTrackerConfig{
        requireSr.value(), requireCname.value(), static_cast<int64_t>(srTimeout.value()) * 1'000'000,
        static_cast<int64_t>(cnameTimeout.value()) * 1'000'000});
    if (sourceClockMappingEnabled) {
        if (!maximumExtrapolation) return ::media::Status::failure(maximumExtrapolation.error());
        auto schedule = MediaRtpClockObservationSchedule::create(
            static_cast<std::int64_t>(srTimeout.value()) * 1'000'000,
            maximumExtrapolation.value(),
            static_cast<std::int64_t>(cnameTimeout.value()) * 1'000'000,
            lossPolicy);
        if (!schedule) return ::media::Status::failure(schedule.error());
        m_clockSchedule = std::make_unique<MediaRtpClockObservationSchedule>(
            std::move(schedule).value());
    }
    m_streamSnapshot = MediaBufferRef(std::move(snapshot).value());
    m_requireCname = requireCname.value();
    m_rtcpCompositionMode = std::move(rtcpComposition).value();
    m_cancellableReadTimeoutMs = readTimeout.value();
    m_initialized = true;
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::processRtp(MediaGraphExecutionContext& context, MediaRtpUdpDatagram datagram)
{
    auto parsed = MediaRtpPacketParser::parse(datagram.bytes);
    if (!parsed) return ::media::Status::failure(parsed.error());
    const std::uint64_t generationBeforeObservation = m_clockTracker->generation();
    const std::int64_t observedAtNs = steadyNowNs();
    m_clockTracker->observeMedia(parsed.value().ssrc, observedAtNs);
    if (auto status = queueClockEvidence(context, observedAtNs); !status) {
        return status;
    }
    auto reordered = m_reorder->push(std::move(parsed).value(), std::chrono::steady_clock::now());
    if (!reordered) return ::media::Status::failure(reordered.error());
    return processReordered(context, std::move(reordered).value(),
                            generationBeforeObservation);
}

::media::Status RawRtpInputNode::processReordered(
    MediaGraphExecutionContext& context,
    MediaRtpReorderResult reordered,
    std::uint64_t generationBeforeObservation)
{
    if (!reordered.discontinuities.empty()) {
        if (m_clockTracker->generation() == generationBeforeObservation) {
            m_clockTracker->observeContinuityLoss();
        }
        if (m_clockSchedule) m_clockSchedule->reset();
    }
    for (const auto& discontinuity : reordered.discontinuities) {
        m_depacketizer->discontinuity(discontinuity.reason);
        if (context.findOutputChannel(nodeId(), "event")) {
            m_events.emplace_back(
                "event",
                makeMediaBufferRef<MediaRtpIngressEventBuffer>(
                    discontinuity, m_clockTracker->generation(), nextIngressSequence()));
        }
    }
    for (const MediaRtpPacket& packet : reordered.packets) {
        auto depacketized = m_depacketizer->push(packet);
        if (!depacketized) return ::media::Status::failure(depacketized.error());
        for (MediaRtpAccessUnit& unit : depacketized.value().accessUnits) {
            const bool depacketizedKey =
                (unit.packet->flags & AV_PKT_FLAG_KEY) != 0;
            auto buffer = FFmpegBufferFactory::wrapPacket(std::move(unit.packet), m_config.streamKind, std::nullopt);
            if (!buffer) return ::media::Status::failure(buffer.error());
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
    }
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::processRtcp(MediaGraphExecutionContext& context, MediaRtpUdpDatagram datagram)
{
    if (!m_rtcpCompositionMode) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "RawRtpInputNode requires planned RTCP composition mode"));
    }
    auto packets = MediaRtcpCompoundParser::parse(datagram.bytes, MediaRtcpCompoundPolicy{
        *m_rtcpCompositionMode, m_requireCname});
    if (!packets) return ::media::Status::failure(packets.error());
    const int64_t observedAtNs = steadyNowNs();
    auto status = m_clockTracker->observe(packets.value(), observedAtNs);
    if (!status) {
        if (m_clockSchedule) m_clockSchedule->reset();
        if (context.findOutputChannel(nodeId(), "event")) {
            m_events.emplace_back(
                "event",
                makeMediaBufferRef<MediaRtpIngressEventBuffer>(
                    MediaRtpClockInvalidation{m_clockTracker->generation()},
                    nextIngressSequence()));
        }
        return ::media::Status::success();
    }
    return queueClockEvidence(context, observedAtNs);
}

::media::Status RawRtpInputNode::queueClockEvidence(
    MediaGraphExecutionContext& context,
    std::int64_t observedAtNs)
{
    auto update = m_clockTracker->takeEvidenceUpdate(observedAtNs);
    if (!update) return ::media::Status::failure(update.error());
    if (!update.value()) return ::media::Status::success();
    const MediaRtcpClockEvidence& evidence = *update.value();
    if (m_clockSchedule) {
        if (auto status = m_clockSchedule->observeEvidence(
                evidence.senderReportObservedAtNs,
                evidence.cnameObservedAtNs); !status) {
            return status;
        }
    }
    if (context.findOutputChannel(nodeId(), "clock")) {
        m_events.emplace_back(
            "clock",
            makeMediaBufferRef<MediaRtpIngressEventBuffer>(
                std::move(*update.value()), nextIngressSequence()));
    }
    return ::media::Status::success();
}

::media::Status RawRtpInputNode::queueClockTransition(
    MediaGraphExecutionContext&,
    std::int64_t observedAtNs)
{
    if (!m_clockSchedule) {
        return ::media::Status::success();
    }
    auto transition = m_clockSchedule->transition(observedAtNs);
    if (!transition) return ::media::Status::failure(transition.error());
    if (!transition.value()) return ::media::Status::success();
    const char* stream = m_config.streamKind == MediaStreamKind::Video
        ? "video"
        : "audio";
    const char* age = *transition.value() == MediaRtpClockAgeTransition::Degraded
        ? "degraded"
        : "expired";
    return ::media::Status::failure(::media::ErrorInfo::ioFailure(
        std::string("RTP ") + stream +
        " source clock evidence " + age +
        "; sender_report_timeout_ns=" +
        std::to_string(m_clockSchedule->senderReportTimeoutNs()) +
        " maximum_extrapolation_ns=" +
        std::to_string(m_clockSchedule->maximumExtrapolationNs()) +
        " cname_timeout_ns=" +
        std::to_string(m_clockSchedule->cnameTimeoutNs())));
}

::media::Status RawRtpInputNode::stop(MediaGraphExecutionContext& context)
{
    auto transportStatus = m_transport.stop();
    m_transport.close();
    resetState();
    auto baseStatus = FFmpegNodeRuntime::stop(context);
    return !transportStatus ? transportStatus : baseStatus;
}

void RawRtpInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    (void)m_transport.abort();
    m_transport.close();
    resetState();
    FFmpegNodeRuntime::abort(context);
}

void RawRtpInputNode::resetState() noexcept
{
    m_reorder.reset();
    m_depacketizer.reset();
    m_clockTracker.reset();
    m_clockSchedule.reset();
    m_config = {};
    m_streamSnapshot.reset();
    m_packets.clear();
    m_events.clear();
    m_preparedDatagrams.clear();
    m_initialized = false;
    m_preparedQueueTraceEmitted = false;
    m_formatEmitted = false;
    m_keyTraceEmitted = false;
    m_requireCname = false;
    m_rtcpCompositionMode.reset();
    m_cancellableReadTimeoutMs = 0;
    m_nextIngressSequence = 1;
}

std::uint64_t RawRtpInputNode::nextIngressSequence() noexcept
{
    return m_nextIngressSequence++;
}

} // namespace media::ffmpeg::graph
