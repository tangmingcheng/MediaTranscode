#include "internal/graph/nodes/output/MediaRtpWirePreparedTransaction.h"
#include "internal/graph/nodes/output/MediaWireCommitPreparation.h"
#include "internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializer.h"

#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"

#include "internal/graph/nodes/output/MediaMpegTsUdpWirePreparationWorkspace.h"
#include <memory>
#include <new>
#include <array>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint16_t ProjectTsPacketBytes = 188;
constexpr int Mp2tStaticPayloadType = 33;
constexpr int Mp2tClockRate = 90'000;


} // namespace

::media::Result<MediaSharedAllocationGeometry>
MediaMpegTsUdpWireDatagramMaterializer::transactionAllocationGeometry()
{
    return MediaDatagramCommitTransaction::allocationGeometry<MediaMpegTsUdpWireCommitTransaction>();
}

MediaMpegTsUdpWireDatagramMaterializer::
MediaMpegTsUdpWireDatagramMaterializer(
    MediaMpegTsUdpWireDatagramMaterializerConfig config) noexcept
    : m_config(std::move(config))
{
}

::media::Result<MediaMpegTsUdpWireDatagramMaterializer>
MediaMpegTsUdpWireDatagramMaterializer::create(
    MediaMpegTsUdpWireDatagramMaterializerConfig config)
{
    using Result =
        ::media::Result<MediaMpegTsUdpWireDatagramMaterializer>;
    if (config.sessionKey.empty() || config.serviceScopeId.empty() ||
        config.generation == 0 || config.endpointId == 0 ||
        config.deadline.endpointId != config.endpointId ||
        !config.globalSequence ||
        config.tsPacketBytes != ProjectTsPacketBytes ||
        config.maximumDatagramBytes < config.tsPacketBytes ||
        (config.globalSequence && !config.globalSequence->commitStorage().protocolPrefixGeometry) ||
        config.batchPlan.maximumDatagrams == 0 ||
        config.batchPlan.maximumBytes < config.maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP wire materializer requires explicit generation, endpoint, 188-byte TS packet, and datagram facts"));
    }
    const auto global = config.globalSequence->snapshot();
    if (config.globalSequence->sessionKey() != config.sessionKey ||
        config.globalSequence->serviceScopeId() != config.serviceScopeId ||
        global.generation != config.generation || global.poisoned ||
        !config.reservationWakeup ||
        global.reservationActive) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP wire materializer global sequence session or service scope identity differs"));
    }
    auto registered = config.globalSequence->registerReservationWakeup(
        config.endpointId, config.reservationWakeup);
    if (!registered) return Result::failure(registered.error());
    return Result::success(MediaMpegTsUdpWireDatagramMaterializer(
        std::move(config)));
}

::media::Status MediaMpegTsUdpWireDatagramMaterializer::prepareProtocolBatch(
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolBatch,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Status;
    if (!m_config.globalSequence || m_failed || !protocolBatch ||
        protocolBatch->generation() != m_config.generation)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP preparation requires an active matching protocol owner"));
    if (m_pending) {
        if (m_pending->protocol != protocolBatch ||
            protocolBatch->materializedDatagrams() != m_pending->protocolOffset ||
            protocolBatch->datagrams().size() < m_pending->entries.size())
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS UDP retry changed its prepared protocol owner or cursor"));
        return Result::success();
    }
    const auto datagrams = protocolBatch->datagrams().first((std::min)(
        protocolBatch->datagrams().size(),
        static_cast<std::size_t>(m_config.batchPlan.maximumDatagrams)));
    if (datagrams.empty()) return Result::failure(::media::ErrorInfo::invalidArgument(
        "MPEG-TS UDP preparation requires a nonempty batch"));
    MediaWireDatagramBatchPartitionBuilder::Sizing sizing(
        m_config.batchPlan, m_config.globalSequence->commitStorage().batchGeometry);
    std::optional<MediaRunningTime> earliestDeadline;
    for (const auto& datagram : datagrams) {
        if (datagram.bytes().empty() || datagram.bytes().size() % m_config.tsPacketBytes ||
            datagram.bytes().size() > m_config.maximumDatagramBytes ||
            datagram.canonicalRelease() < MediaRunningTime::fromNanoseconds(0))
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS UDP preparation requires complete bounded TS datagrams"));
        auto deadline = m_config.deadline.canonicalDeadline(
            datagram.canonicalRelease(), materializedAt, datagram.completionLimit());
        if (!deadline) return Result::failure(deadline.error());
        auto sized = sizing.append(datagram.bytes().size(), deadline.value());
        if (!sized) return sized;
        earliestDeadline = earliestDeadline
            ? (std::min)(*earliestDeadline, deadline.value()) : deadline.value();
    }
    auto outputCost = sizing.storageFootprint();
    auto workspaceCost = MediaMpegTsUdpWirePreparationWorkspace::storageFootprint(datagrams.size());
    if (!outputCost || !workspaceCost)
        return Result::failure(!outputCost ? outputCost.error() : workspaceCost.error());
    auto lease = MediaWireCommitPreparation::admit(m_config.globalSequence,
        datagrams.size(), outputCost.value(), workspaceCost.value());
    if (!lease) return Result::failure(lease.error());
    auto workspace = MediaMpegTsUdpWirePreparationWorkspace::prepare(
        m_config.globalSequence->storageResource(), lease.value(), datagrams.size());
    if (!workspace) return Result::failure(workspace.error());
    for (const auto& datagram : datagrams) {
        auto deadline = m_config.deadline.canonicalDeadline(
            datagram.canonicalRelease(), materializedAt, datagram.completionLimit());
        if (!deadline) return Result::failure(deadline.error());
        workspace.value().globalEntries.push_back({m_config.endpointId,
            static_cast<std::uint64_t>(datagram.bytes().size()), materializedAt});
        workspace.value().outputInputs.push_back({MediaByteSerializationView::fromBytes(datagram.bytes()),
            m_config.endpointId, datagram.canonicalRelease(), deadline.value(), datagram.mediaBoundary(),
            MediaWireProtocolDeadlineObservation{datagram.canonicalDeadline(), datagram.completionLimit(), materializedAt,
                protocolBatch->publicationObservation(), false}});
    }
    auto storage = MediaWireCommitPreparation::fromReservation(m_config.globalSequence,
        workspace.value().outputInputs.view(), m_config.batchPlan, std::move(lease).value());
    if (!storage) return Result::failure(storage.error());
    auto control = MediaDatagramCommitTransaction::prepare<MediaMpegTsUdpWireCommitTransaction>(
        m_config.generation, m_config.globalSequence->commitStorage(),
        m_config.globalSequence->storageResource(), std::move(storage.value().control));
    if (!control) return Result::failure(control.error());
    const auto offset = protocolBatch->materializedDatagrams();
    auto registered = protocolBatch->registerPreparedWire(workspace.value().outputInputs.view(),
        workspace.value().globalEntries.view(), 0);
    if (!registered) return Result::failure(registered.error());
    m_pending.emplace(MediaMpegTsUdpWirePreparedTransaction{
        std::move(protocolBatch), offset, materializedAt, *earliestDeadline,
        std::move(workspace.value().globalEntries), std::move(storage).value(), std::move(control).value()});
    return Result::success();
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsUdpWireDatagramMaterializer::complete(MediaRunningTime now)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (!m_config.globalSequence || m_failed || !m_pending || !m_pending->protocol ||
        m_pending->protocol->generation() != m_config.generation ||
        m_pending->protocol->materializedDatagrams() != m_pending->protocolOffset ||
        m_pending->protocol->datagrams().size() < m_pending->entries.size())
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP completion requires its active unchanged preparation"));
    if (now < m_pending->materializedAt || now > m_pending->deadline)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP prepared transaction exceeded its immutable emission deadline"));
    if (!m_pending->storage.protocolPrefix)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP completion requires prepared protocol prefix storage"));
    auto global = m_config.globalSequence->reserve(m_pending->entries.view(), m_pending->storage.global);
    if (!global) return Result::failure(global.error());
    // Past global admission, any failure is terminal. Never retry consumed state.
    auto pending = std::move(*m_pending);
    m_pending.reset();
    m_failed = true;
    auto protocol = pending.protocol->takeCommitTransaction(
        pending.entries.size(), std::move(*pending.storage.protocolPrefix),
        m_config.globalSequence, global.value(), 0);
    if (!protocol) return Result::failure(protocol.error());
    pending.protocol.reset();
    if (protocol.value().size() != pending.entries.size())
        return Result::failure(::media::ErrorInfo::internalError(
            "MPEG-TS UDP protocol transaction cardinality differs"));
    auto transaction = std::move(pending.control).bind(MediaMpegTsUdpWireCommitTransaction(
        std::move(global).value(), std::move(protocol).value()));
    if (!transaction) return Result::failure(transaction.error());
    auto output = std::move(pending.storage.output).bind(std::move(transaction).value());
    if (!output) return Result::failure(output.error());
    m_failed = false;
    return output;
}

MediaMpegTsRtpWireDatagramMaterializer::
MediaMpegTsRtpWireDatagramMaterializer(
    MediaMpegTsRtpPacketizer packetizer,
    MediaRtpWireDatagramMaterializer rtpMaterializer) noexcept
    : m_packetizer(std::move(packetizer)),
      m_rtpMaterializer(std::move(rtpMaterializer))
{
}

::media::Result<MediaMpegTsRtpWireDatagramMaterializer>
MediaMpegTsRtpWireDatagramMaterializer::create(
    MediaMpegTsRtpWireDatagramMaterializerConfig config)
{
    using Result =
        ::media::Result<MediaMpegTsRtpWireDatagramMaterializer>;
    if (config.payloadType != Mp2tStaticPayloadType ||
        config.clockRate != Mp2tClockRate) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS RTP wire materializer requires the planner-owned MP2T PT 33 and 90 kHz contract"));
    }
    auto packetizer = MediaMpegTsRtpPacketizer::create(
        MediaMpegTsRtpPacketizerConfig{
            config.payloadType,
            config.clockRate,
            config.ssrc,
            config.baseTimestamp,
            config.maximumTsPackets,
            config.maximumDatagramBytes,
            config.masterOrigin});
    if (!packetizer) return Result::failure(packetizer.error());
    auto identity = MediaRtpDatagramRewriteIdentity::create(
        config.payloadType, config.ssrc);
    if (!identity) return Result::failure(identity.error());
    auto mapper = MediaRtpOutputClockMapper::create(
        config.clockRate, config.baseTimestamp, config.masterOrigin);
    if (!mapper) return Result::failure(mapper.error());
    auto rtpMaterializer = MediaRtpWireDatagramMaterializer::create(
        MediaRtpWireDatagramMaterializerConfig{
            std::move(config.sessionKey),
            std::move(config.serviceScopeId),
            config.generation,
            config.rtpEndpointId,
            config.rtcpEndpointId,
            config.rtpDeadline,
            config.rtcpDeadline,
            std::move(config.globalSequence),
            identity.value(),
            mapper.value(),
            config.ntpEpoch,
            std::move(config.senderReportSchedule),
            std::move(config.cname),
            config.initialRtpSequence,
            0,
            0,
            config.maximumDatagramBytes,
            config.maximumOutstandingDatagrams,
            config.batchPlan,
            std::move(config.reservationWakeup)});
    if (!rtpMaterializer) return Result::failure(rtpMaterializer.error());
    return Result::success(MediaMpegTsRtpWireDatagramMaterializer(
        std::move(packetizer).value(),
        std::move(rtpMaterializer).value()));
}

::media::Result<MediaRtpWirePreparedTransaction>
MediaMpegTsRtpWireDatagramMaterializer::prepareProtocolBatch(
    std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolBatch,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Result<MediaRtpWirePreparedTransaction>;
    if (!protocolBatch || protocolBatch->generation() != m_rtpMaterializer.generation())
        return Result::failure(::media::ErrorInfo::invalidArgument("MPEG-TS RTP protocol batch generation differs"));
    const auto datagrams = protocolBatch->datagrams().first((std::min)(
        protocolBatch->datagrams().size(), m_rtpMaterializer.maximumBatchDatagrams()));
    return m_rtpMaterializer.prepareBatch(
        MediaRtpWireInputBatch(datagrams, std::cref(m_packetizer)), std::move(protocolBatch), materializedAt, nullptr);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsRtpWireDatagramMaterializer::complete(
    MediaRtpWirePreparedTransaction& pending, MediaRunningTime now)
{
    return m_rtpMaterializer.complete(pending, now);
}

::media::Result<std::optional<MediaRtpWirePreparedTransaction>>
MediaMpegTsRtpWireDatagramMaterializer::prepareTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    return m_rtpMaterializer.prepareTerminalReport(
        reportInstant, canonicalRelease, materializedAt);
}

::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
MediaMpegTsRtpWireDatagramMaterializer::snapshot() const noexcept
{
    return m_rtpMaterializer.snapshot();
}

} // namespace media::ffmpeg::graph
