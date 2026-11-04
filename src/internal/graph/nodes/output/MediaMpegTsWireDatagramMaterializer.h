#pragma once
#include "internal/graph/nodes/output/MediaMpegTsUdpWirePreparedTransaction.h"

#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaMpegTsUdpWireDatagramMaterializerConfig final {
    std::string sessionKey;
    std::string serviceScopeId;
    std::uint64_t generation;
    std::uint64_t endpointId;
    MediaDatagramWireDeadlinePlan deadline;
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence;
    std::uint16_t tsPacketBytes;
    std::size_t maximumDatagramBytes;
    MediaDatagramBatchPlan batchPlan;
    std::shared_ptr<MediaNodeWakeup> reservationWakeup;
};

class MediaMpegTsUdpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaSharedAllocationGeometry> transactionAllocationGeometry();
    static ::media::Result<MediaMpegTsUdpWireDatagramMaterializer> create(
        MediaMpegTsUdpWireDatagramMaterializerConfig config);

    MediaMpegTsUdpWireDatagramMaterializer(const MediaMpegTsUdpWireDatagramMaterializer&) = delete;
    MediaMpegTsUdpWireDatagramMaterializer& operator=(const MediaMpegTsUdpWireDatagramMaterializer&) = delete;
    MediaMpegTsUdpWireDatagramMaterializer(MediaMpegTsUdpWireDatagramMaterializer&&) noexcept = default;
    MediaMpegTsUdpWireDatagramMaterializer& operator=(MediaMpegTsUdpWireDatagramMaterializer&&) = delete;
    ::media::Status prepareProtocolBatch(
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolBatch,
        MediaRunningTime materializedAt);
    ::media::Result<MediaWireDatagramBatchCollection> complete(MediaRunningTime now);

private:
    explicit MediaMpegTsUdpWireDatagramMaterializer(
        MediaMpegTsUdpWireDatagramMaterializerConfig config) noexcept;
    MediaMpegTsUdpWireDatagramMaterializerConfig m_config;
    std::optional<MediaMpegTsUdpWirePreparedTransaction> m_pending;
    bool m_failed = false;
};

struct MediaMpegTsRtpWireDatagramMaterializerConfig final {
    std::string sessionKey;
    std::string serviceScopeId;
    std::uint64_t generation;
    std::uint64_t rtpEndpointId;
    std::uint64_t rtcpEndpointId;
    MediaDatagramWireDeadlinePlan rtpDeadline;
    MediaDatagramWireDeadlinePlan rtcpDeadline;
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence;
    int payloadType;
    int clockRate;
    std::uint32_t ssrc;
    std::uint32_t baseTimestamp;
    std::uint16_t initialRtpSequence;
    std::uint16_t maximumTsPackets;
    std::size_t maximumDatagramBytes;
    std::size_t maximumOutstandingDatagrams;
    MediaDatagramBatchPlan batchPlan;
    MediaRunningTime masterOrigin;
    MediaSharedNtpEpoch ntpEpoch;
    MediaRtcpSenderReportSchedule senderReportSchedule;
    std::string cname;
    std::shared_ptr<MediaNodeWakeup> reservationWakeup;
};

class MediaMpegTsRtpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaMpegTsRtpWireDatagramMaterializer> create(
        MediaMpegTsRtpWireDatagramMaterializerConfig config);

    ::media::Result<MediaRtpWirePreparedTransaction> prepareProtocolBatch(
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolBatch,
        MediaRunningTime materializedAt);
    ::media::Result<MediaWireDatagramBatchCollection> complete(
        MediaRtpWirePreparedTransaction& pending, MediaRunningTime now);

    ::media::Result<std::optional<MediaRtpWirePreparedTransaction>>
    prepareTerminalReport(
        MediaRunningTime reportInstant,
        MediaRunningTime canonicalRelease,
        MediaRunningTime materializedAt);

    ::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
    snapshot() const noexcept;

private:
    MediaMpegTsRtpWireDatagramMaterializer(
        MediaMpegTsRtpPacketizer packetizer,
        MediaRtpWireDatagramMaterializer rtpMaterializer) noexcept;

    MediaMpegTsRtpPacketizer m_packetizer;
    MediaRtpWireDatagramMaterializer m_rtpMaterializer;
};

} // namespace media::ffmpeg::graph
