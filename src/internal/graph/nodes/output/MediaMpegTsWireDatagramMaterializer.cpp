#include "internal/graph/nodes/output/MediaMpegTsWireDatagramMaterializer.h"

#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuilder.h"

#include <memory>
#include <new>
#include <array>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

constexpr std::uint16_t ProjectTsPacketBytes = 188;
constexpr int Mp2tStaticPayloadType = 33;
constexpr int Mp2tClockRate = 90'000;

class MediaMpegTsUdpWireCommitTransaction final {
public:
    explicit MediaMpegTsUdpWireCommitTransaction(
        MediaWireGlobalSequenceReservation reservation,
        std::optional<MediaProtocolDatagramCommitTransaction>
            protocolCommit) noexcept
        : m_reservation(std::move(reservation)),
          m_protocolCommit(std::move(protocolCommit))
    {
    }

    std::size_t size() const noexcept { return m_reservation.size(); }

    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept
    {
        return m_reservation.sequence(index);
    }

    ::media::Status markScheduledPrefix(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept
    {
        if (begin > size() || count > size() - begin) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP schedule prefix is outside its transaction"));
        }
        return m_reservation.markScheduled(begin, count, now);
    }

    ::media::Status commitSubmittedPrefix(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept
    {
        if (begin > size() || count > size() - begin) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP submitted prefix is outside its transaction"));
        }
        if (m_protocolCommit &&
            (!m_protocolCommit->valid() ||
             count > m_protocolCommit->size() -
                         m_protocolCommit->committed())) {
            return ::media::Status::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP submitted prefix exceeds its protocol transaction"));
        }
        auto committed = m_reservation.commit(begin, count, now);
        if (!committed) return committed;
        if (m_protocolCommit) {
            auto protocolCommitted = m_protocolCommit->commitNextPrefix(count);
            if (!protocolCommitted) return protocolCommitted;
        }
        return ::media::Status::success();
    }

private:
    MediaWireGlobalSequenceReservation m_reservation;
    std::optional<MediaProtocolDatagramCommitTransaction> m_protocolCommit;
};

} // namespace

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
        config.batchPlan.maximumDatagrams == 0 ||
        config.batchPlan.maximumBytes < config.maximumDatagramBytes) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP wire materializer requires explicit generation, endpoint, 188-byte TS packet, and datagram facts"));
    }
    const auto global = config.globalSequence->snapshot();
    if (config.globalSequence->sessionKey() != config.sessionKey ||
        config.globalSequence->serviceScopeId() != config.serviceScopeId ||
        global.generation != config.generation || global.poisoned ||
        global.reservationActive) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP wire materializer global sequence session or service scope identity differs"));
    }
    return Result::success(MediaMpegTsUdpWireDatagramMaterializer(
        std::move(config)));
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsUdpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    const std::array<MediaMpegTsDatagramView, 1> datagrams{{
        {completeTsPackets, canonicalRelease}}};
    return materializeBatch(datagrams, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsUdpWireDatagramMaterializer::materializeBatch(
    std::span<const MediaMpegTsDatagramView> datagrams,
    MediaRunningTime materializedAt)
{
    return materializeBatchReserved(datagrams, nullptr, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsUdpWireDatagramMaterializer::materializeProtocolBatch(
    MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (protocolBatch.generation() != m_config.generation) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP protocol batch generation differs"));
    }
    std::vector<MediaMpegTsDatagramView> views;
    try {
        views.reserve(protocolBatch.datagrams().size());
        for (std::size_t index = 0;
             index < protocolBatch.datagrams().size(); ++index) {
            const auto& datagram = protocolBatch.datagrams()[index];
            views.push_back(MediaMpegTsDatagramView{
                datagram.bytes(), datagram.canonicalRelease()});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS UDP protocol batch views"));
    }
    return materializeBatchReserved(
        views, &protocolBatch, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsUdpWireDatagramMaterializer::materializeBatchReserved(
    std::span<const MediaMpegTsDatagramView> datagrams,
    MediaMpegTsProtocolDatagramBatchBuffer* protocolBatch,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (datagrams.empty() ||
        (protocolBatch &&
         protocolBatch->datagrams().size() != datagrams.size())) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP wire materializer requires a nonempty batch"));
    }
    std::vector<MediaRunningTime> deadlines;
    try {
        deadlines.reserve(datagrams.size());
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS UDP wire deadlines"));
    }
    for (const auto& datagram : datagrams) {
        if (datagram.completeTsPackets.empty() ||
            datagram.completeTsPackets.size() % m_config.tsPacketBytes != 0 ||
            datagram.completeTsPackets.size() > m_config.maximumDatagramBytes ||
            datagram.canonicalRelease < MediaRunningTime::fromNanoseconds(0)) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS UDP wire materializer requires complete TS datagrams and ordered canonical windows"));
        }
        auto deadline = m_config.deadline.canonicalDeadline(
            datagram.canonicalRelease);
        if (!deadline) return Result::failure(deadline.error());
        deadlines.push_back(deadline.value());
    }
    std::vector<MediaWireGlobalSequenceReservationEntry> globalEntries;
    try {
        globalEntries.reserve(datagrams.size());
        for (const auto& datagram : datagrams) {
            globalEntries.push_back({
                m_config.endpointId,
                static_cast<std::uint64_t>(
                    datagram.completeTsPackets.size()),
                materializedAt});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS UDP service backlog reservation"));
    }
    auto global = m_config.globalSequence->reserve(globalEntries);
    if (!global) return Result::failure(global.error());
    std::optional<MediaProtocolDatagramCommitTransaction> protocolCommit;
    if (protocolBatch) {
        auto transaction = protocolBatch->takeCommitTransaction();
        if (!transaction) return Result::failure(transaction.error());
        if (transaction.value().size() != datagrams.size()) {
            return Result::failure(::media::ErrorInfo::internalError(
                "MPEG-TS UDP protocol transaction cardinality differs"));
        }
        protocolCommit.emplace(std::move(transaction).value());
    }
    auto transaction = MediaDatagramCommitTransaction::create(
        m_config.generation,
        MediaMpegTsUdpWireCommitTransaction(
            std::move(global).value(), std::move(protocolCommit)));
    if (!transaction) return Result::failure(transaction.error());
    const auto firstGlobalSequence = transaction.value().firstGlobalSequence();
    auto builderResult = MediaWireDatagramBatchPartitionBuilder::create(
        m_config.sessionKey, m_config.serviceScopeId, m_config.generation,
        m_config.batchPlan, std::move(transaction).value());
    if (!builderResult) return Result::failure(builderResult.error());
    auto builder = std::move(builderResult).value();
    for (std::size_t index = 0; index < datagrams.size(); ++index) {
        const auto sequence = firstGlobalSequence +
            static_cast<std::uint64_t>(index);
        auto appended = builder.append(
            datagrams[index].completeTsPackets, m_config.endpointId,
            datagrams[index].canonicalRelease,
            deadlines[index], sequence);
        if (!appended) return Result::failure(appended.error());
    }
    return builder.finish();
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
            config.batchPlan});
    if (!rtpMaterializer) return Result::failure(rtpMaterializer.error());
    return Result::success(MediaMpegTsRtpWireDatagramMaterializer(
        std::move(packetizer).value(),
        std::move(rtpMaterializer).value()));
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsRtpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    const std::array<MediaMpegTsDatagramView, 1> datagrams{{
        {completeTsPackets, canonicalRelease}}};
    return materializeBatch(datagrams, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsRtpWireDatagramMaterializer::materializeBatch(
    std::span<const MediaMpegTsDatagramView> datagrams,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (datagrams.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS RTP wire materializer requires a nonempty batch"));
    }
    std::vector<std::vector<std::uint8_t>> payloads;
    std::vector<std::size_t> payloadOctets;
    std::vector<MediaPacketizedRtpDatagramView> packetized;
    try {
        payloads.reserve(datagrams.size());
        payloadOctets.reserve(datagrams.size());
        packetized.reserve(datagrams.size());
        for (const auto& datagram : datagrams) {
            auto packet = m_packetizer.packetize(
                datagram.completeTsPackets,
                datagram.canonicalRelease, 0);
            if (!packet) return Result::failure(packet.error());
            payloadOctets.push_back(packet.value().payloadOctets());
            payloads.push_back(packet.value().releaseDatagram());
        }
        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            packetized.push_back(MediaPacketizedRtpDatagramView{
                payloads[index], payloadOctets[index],
                datagrams[index].canonicalRelease,
                datagrams[index].canonicalRelease});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS RTP wire batch"));
    }
    return m_rtpMaterializer.materializeBatch(
        packetized, materializedAt);
}

::media::Result<MediaWireDatagramBatchCollection>
MediaMpegTsRtpWireDatagramMaterializer::materializeProtocolBatch(
    MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch,
    MediaRunningTime materializedAt)
{
    using Result = ::media::Result<MediaWireDatagramBatchCollection>;
    if (protocolBatch.generation() != m_rtpMaterializer.generation()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS RTP protocol batch generation differs"));
    }
    std::vector<std::vector<std::uint8_t>> payloads;
    std::vector<std::size_t> payloadOctets;
    std::vector<MediaPacketizedRtpDatagramView> packetized;
    try {
        const auto datagrams = protocolBatch.datagrams();
        payloads.reserve(datagrams.size());
        payloadOctets.reserve(datagrams.size());
        packetized.reserve(datagrams.size());
        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            const auto& datagram = datagrams[index];
            auto packet = m_packetizer.packetize(
                datagram.bytes(), datagram.canonicalRelease(), 0);
            if (!packet) return Result::failure(packet.error());
            payloadOctets.push_back(packet.value().payloadOctets());
            payloads.push_back(packet.value().releaseDatagram());
        }
        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            packetized.push_back(MediaPacketizedRtpDatagramView{
                payloads[index], payloadOctets[index],
                datagrams[index].canonicalRelease(),
                datagrams[index].canonicalRelease()});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS RTP protocol batch"));
    }
    return m_rtpMaterializer.materializeProtocolBatch(
        packetized, protocolBatch, materializedAt);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materializeTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease,
    MediaRunningTime materializedAt)
{
    return m_rtpMaterializer.materializeTerminalReport(
        reportInstant, canonicalRelease, materializedAt);
}

::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
MediaMpegTsRtpWireDatagramMaterializer::snapshot() const noexcept
{
    return m_rtpMaterializer.snapshot();
}

} // namespace media::ffmpeg::graph
