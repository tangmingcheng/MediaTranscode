#pragma once

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

struct MediaMpegTsDatagramView final {
    std::span<const std::uint8_t> completeTsPackets;
    MediaRunningTime canonicalRelease;
};

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
};

class MediaMpegTsUdpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaMpegTsUdpWireDatagramMaterializer> create(
        MediaMpegTsUdpWireDatagramMaterializerConfig config);

    ::media::Result<MediaWireDatagramBatchCollection> materialize(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime canonicalRelease);
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeBatch(std::span<const MediaMpegTsDatagramView> datagrams);
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeProtocolBatch(
        MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch);

private:
    explicit MediaMpegTsUdpWireDatagramMaterializer(
        MediaMpegTsUdpWireDatagramMaterializerConfig config) noexcept;
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeBatchReserved(
        std::span<const MediaMpegTsDatagramView> datagrams,
        MediaMpegTsProtocolDatagramBatchBuffer* protocolBatch);

    MediaMpegTsUdpWireDatagramMaterializerConfig m_config;
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
};

class MediaMpegTsRtpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaMpegTsRtpWireDatagramMaterializer> create(
        MediaMpegTsRtpWireDatagramMaterializerConfig config);

    ::media::Result<MediaWireDatagramBatchCollection> materialize(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime canonicalRelease);
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeBatch(std::span<const MediaMpegTsDatagramView> datagrams);
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeProtocolBatch(
        MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
    materializeTerminalReport(
        MediaRunningTime reportInstant,
        MediaRunningTime canonicalRelease);

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
