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
        std::vector<MediaProtocolDatagramCommitLease> protocolCommits) noexcept
        : m_reservation(std::move(reservation)),
          m_protocolCommits(std::move(protocolCommits))
    {
    }

    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept
    {
        return m_reservation.sequence(index);
    }

    ::media::Status commit(std::size_t index) noexcept
    {
        auto ready = m_reservation.canCommit(index);
        if (!ready) return ready;
        if (!m_protocolCommits.empty()) {
            if (index >= m_protocolCommits.size()) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::internalError(
                        "MPEG-TS UDP protocol commit index is outside its transaction"));
            }
            auto protocolCommitted = m_protocolCommits[index].commit();
            if (!protocolCommitted) return protocolCommitted;
        }
        return m_reservation.commit(index);
    }

private:
    MediaWireGlobalSequenceReservation m_reservation;
    std::vector<MediaProtocolDatagramCommitLease> m_protocolCommits;
};

class MediaMpegTsUdpWireEntryReservation final {
public:
    explicit MediaMpegTsUdpWireEntryReservation(
        std::shared_ptr<MediaMpegTsUdpWireCommitTransaction> transaction,
        std::size_t index)
        noexcept
        : m_transaction(std::move(transaction)), m_index(index)
    {
    }

    ::media::Status commit() noexcept
    {
        return m_transaction
            ? m_transaction->commit(m_index)
            : ::media::Status::failure(::media::ErrorInfo::internalError(
                  "MPEG-TS UDP wire reservation has no transaction"));
    }

private:
    std::shared_ptr<MediaMpegTsUdpWireCommitTransaction> m_transaction;
    std::size_t m_index;
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
        config.maximumDatagramBytes < config.tsPacketBytes) {
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

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsUdpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime canonicalRelease)
{
    const std::array<MediaMpegTsDatagramView, 1> datagrams{{
        {completeTsPackets, canonicalRelease, canonicalRelease}}};
    return materializeBatch(datagrams);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsUdpWireDatagramMaterializer::materializeBatch(
    std::span<const MediaMpegTsDatagramView> datagrams)
{
    return materializeBatchReserved(datagrams, nullptr);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsUdpWireDatagramMaterializer::materializeProtocolBatch(
    MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
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
                datagram.bytes(), datagram.presentationOnMaster(),
                datagram.canonicalRelease()});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS UDP protocol batch views"));
    }
    return materializeBatchReserved(views, &protocolBatch);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsUdpWireDatagramMaterializer::materializeBatchReserved(
    std::span<const MediaMpegTsDatagramView> datagrams,
    MediaMpegTsProtocolDatagramBatchBuffer* protocolBatch)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
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
    auto global = m_config.globalSequence->reserve(datagrams.size());
    if (!global) return Result::failure(global.error());
    std::vector<MediaProtocolDatagramCommitLease> protocolCommits;
    if (protocolBatch) {
        try {
            protocolCommits.reserve(datagrams.size());
            for (std::size_t index = 0; index < datagrams.size(); ++index) {
                auto lease = protocolBatch->takeCommitLease(index);
                if (!lease) return Result::failure(lease.error());
                protocolCommits.push_back(std::move(lease).value());
            }
        } catch (const std::bad_alloc&) {
            return Result::failure(::media::ErrorInfo::allocationFailed(
                "MPEG-TS UDP protocol commit transfer"));
        }
    }
    std::shared_ptr<MediaMpegTsUdpWireCommitTransaction> transaction;
    try {
        transaction =
                std::make_shared<MediaMpegTsUdpWireCommitTransaction>(
                std::move(global).value(), std::move(protocolCommits));
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS UDP wire commit transaction"));
    }
    auto builderResult = MediaWireDatagramBatchBuilder::create(
        m_config.sessionKey, m_config.serviceScopeId, m_config.generation);
    if (!builderResult) return Result::failure(builderResult.error());
    auto builder = std::move(builderResult).value();
    for (std::size_t index = 0; index < datagrams.size(); ++index) {
        auto sequence = transaction->sequence(index);
        if (!sequence) return Result::failure(sequence.error());
        auto lease = MediaDatagramSubmitCommitLease::create(
            m_config.generation, sequence.value(),
            MediaMpegTsUdpWireEntryReservation(transaction, index));
        if (!lease) return Result::failure(lease.error());
        auto appended = builder.append(
            datagrams[index].completeTsPackets, m_config.endpointId,
            datagrams[index].canonicalRelease,
            deadlines[index], sequence.value(),
            std::move(lease).value());
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
            config.maximumOutstandingDatagrams});
    if (!rtpMaterializer) return Result::failure(rtpMaterializer.error());
    return Result::success(MediaMpegTsRtpWireDatagramMaterializer(
        std::move(packetizer).value(),
        std::move(rtpMaterializer).value()));
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease)
{
    const std::array<MediaMpegTsDatagramView, 1> datagrams{{
        {completeTsPackets, presentationOnMaster,
         canonicalRelease}}};
    return materializeBatch(datagrams);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materializeBatch(
    std::span<const MediaMpegTsDatagramView> datagrams)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
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
                datagram.presentationOnMaster, 0);
            if (!packet) return Result::failure(packet.error());
            payloadOctets.push_back(packet.value().payloadOctets());
            payloads.push_back(packet.value().releaseDatagram());
        }
        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            packetized.push_back(MediaPacketizedRtpDatagramView{
                payloads[index], payloadOctets[index],
                datagrams[index].presentationOnMaster,
                datagrams[index].canonicalRelease});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS RTP wire batch"));
    }
    return m_rtpMaterializer.materializeBatch(packetized);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materializeProtocolBatch(
    MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
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
                datagram.bytes(), datagram.presentationOnMaster(), 0);
            if (!packet) return Result::failure(packet.error());
            payloadOctets.push_back(packet.value().payloadOctets());
            payloads.push_back(packet.value().releaseDatagram());
        }
        for (std::size_t index = 0; index < datagrams.size(); ++index) {
            packetized.push_back(MediaPacketizedRtpDatagramView{
                payloads[index], payloadOctets[index],
                datagrams[index].presentationOnMaster(),
                datagrams[index].canonicalRelease()});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS RTP protocol batch"));
    }
    return m_rtpMaterializer.materializeProtocolBatch(
        packetized, protocolBatch);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materializeTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease)
{
    return m_rtpMaterializer.materializeTerminalReport(
        reportInstant, canonicalRelease);
}

::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
MediaMpegTsRtpWireDatagramMaterializer::snapshot() const noexcept
{
    return m_rtpMaterializer.snapshot();
}

} // namespace media::ffmpeg::graph
