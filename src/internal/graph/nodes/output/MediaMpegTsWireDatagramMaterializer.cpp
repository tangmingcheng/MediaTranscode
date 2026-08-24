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
        MediaWireGlobalSequenceReservation reservation) noexcept
        : m_reservation(std::move(reservation))
    {
    }

    ::media::Result<std::uint64_t> sequence(std::size_t index) const noexcept
    {
        return m_reservation.sequence(index);
    }

    ::media::Status commit(std::size_t index) noexcept
    {
        return m_reservation.commit(index);
    }

private:
    MediaWireGlobalSequenceReservation m_reservation;
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
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline)
{
    const std::array<MediaMpegTsDatagramView, 1> datagrams{{
        {completeTsPackets, canonicalRelease, canonicalRelease,
         canonicalDeadline}}};
    return materializeBatch(datagrams);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsUdpWireDatagramMaterializer::materializeBatch(
    std::span<const MediaMpegTsDatagramView> datagrams)
{
    using Result =
        ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>;
    if (datagrams.empty()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "MPEG-TS UDP wire materializer requires a nonempty batch"));
    }
    for (const auto& datagram : datagrams) {
        if (datagram.completeTsPackets.empty() ||
            datagram.completeTsPackets.size() % m_config.tsPacketBytes != 0 ||
            datagram.completeTsPackets.size() > m_config.maximumDatagramBytes ||
            datagram.canonicalRelease < MediaRunningTime::fromNanoseconds(0) ||
            datagram.canonicalDeadline < datagram.canonicalRelease) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "MPEG-TS UDP wire materializer requires complete TS datagrams and ordered canonical windows"));
        }
    }
    auto global = m_config.globalSequence->reserve(datagrams.size());
    if (!global) return Result::failure(global.error());
    std::shared_ptr<MediaMpegTsUdpWireCommitTransaction> transaction;
    try {
        transaction =
            std::make_shared<MediaMpegTsUdpWireCommitTransaction>(
                std::move(global).value());
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
            datagrams[index].canonicalDeadline, sequence.value(),
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
            std::move(config.globalSequence),
            identity.value(),
            mapper.value(),
            config.ntpEpoch,
            std::move(config.senderReportSchedule),
            std::move(config.cname),
            config.initialRtpSequence,
            0,
            0,
            config.maximumDatagramBytes});
    if (!rtpMaterializer) return Result::failure(rtpMaterializer.error());
    return Result::success(MediaMpegTsRtpWireDatagramMaterializer(
        std::move(packetizer).value(),
        std::move(rtpMaterializer).value()));
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materialize(
    std::span<const std::uint8_t> completeTsPackets,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline)
{
    const std::array<MediaMpegTsDatagramView, 1> datagrams{{
        {completeTsPackets, presentationOnMaster,
         canonicalRelease, canonicalDeadline}}};
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
                datagrams[index].canonicalRelease,
                datagrams[index].canonicalDeadline});
        }
    } catch (const std::bad_alloc&) {
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "MPEG-TS RTP wire batch"));
    }
    return m_rtpMaterializer.materializeBatch(packetized);
}

::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
MediaMpegTsRtpWireDatagramMaterializer::materializeTerminalReport(
    MediaRunningTime reportInstant,
    MediaRunningTime canonicalRelease,
    MediaRunningTime canonicalDeadline)
{
    return m_rtpMaterializer.materializeTerminalReport(
        reportInstant, canonicalRelease, canonicalDeadline);
}

::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
MediaMpegTsRtpWireDatagramMaterializer::snapshot() const noexcept
{
    return m_rtpMaterializer.snapshot();
}

} // namespace media::ffmpeg::graph
