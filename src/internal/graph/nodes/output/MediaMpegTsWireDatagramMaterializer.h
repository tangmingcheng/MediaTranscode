#pragma once

#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"
#include "internal/graph/protocol/rtp/MediaMpegTsRtpPacketizer.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

struct MediaMpegTsUdpWireDatagramMaterializerConfig final {
    std::uint64_t generation;
    std::uint64_t endpointId;
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence;
    std::uint16_t tsPacketBytes;
    std::size_t maximumDatagramBytes;
};

class MediaMpegTsUdpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaMpegTsUdpWireDatagramMaterializer> create(
        MediaMpegTsUdpWireDatagramMaterializerConfig config);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>> materialize(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline);

private:
    explicit MediaMpegTsUdpWireDatagramMaterializer(
        MediaMpegTsUdpWireDatagramMaterializerConfig config) noexcept;

    MediaMpegTsUdpWireDatagramMaterializerConfig m_config;
};

struct MediaMpegTsRtpWireDatagramMaterializerConfig final {
    std::uint64_t generation;
    std::uint64_t rtpEndpointId;
    std::uint64_t rtcpEndpointId;
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence;
    int payloadType;
    int clockRate;
    std::uint32_t ssrc;
    std::uint32_t baseTimestamp;
    std::uint16_t initialRtpSequence;
    std::uint8_t maximumTsPackets;
    std::size_t maximumDatagramBytes;
    MediaRunningTime masterOrigin;
    MediaSharedNtpEpoch ntpEpoch;
    MediaRtcpSenderReportSchedule senderReportSchedule;
    std::string cname;
};

class MediaMpegTsRtpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaMpegTsRtpWireDatagramMaterializer> create(
        MediaMpegTsRtpWireDatagramMaterializerConfig config);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>> materialize(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
    materializeTerminalReport(
        MediaRunningTime reportInstant,
        MediaRunningTime canonicalRelease,
        MediaRunningTime canonicalDeadline);

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
