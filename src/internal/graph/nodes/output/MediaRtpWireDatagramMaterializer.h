#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpSenderReportSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpDatagramRewriter.h"
#include "internal/graph/protocol/rtp/MediaRtpOutputClockMapper.h"
#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramBatchPartitionBuilder.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"
#include "internal/graph/runtime/buffer/MediaProtocolDatagramCommitLease.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtpWireDatagramMaterializerConfig final {
    std::string sessionKey;
    std::string serviceScopeId;
    std::uint64_t generation;
    std::uint64_t rtpEndpointId;
    std::uint64_t rtcpEndpointId;
    MediaDatagramWireDeadlinePlan rtpDeadline;
    MediaDatagramWireDeadlinePlan rtcpDeadline;
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence;
    MediaRtpDatagramRewriteIdentity identity;
    MediaRtpOutputClockMapper clockMapper;
    MediaSharedNtpEpoch ntpEpoch;
    MediaRtcpSenderReportSchedule senderReportSchedule;
    std::string cname;
    std::uint16_t initialRtpSequence;
    std::uint64_t initialPacketCount;
    std::uint64_t initialOctetCount;
    std::size_t maximumDatagramBytes;
    std::size_t maximumOutstandingDatagrams;
    MediaDatagramBatchPlan batchPlan;
};

struct MediaRtpWireDatagramMaterializerSnapshot final {
    std::uint16_t nextRtpSequence;
    std::uint64_t packetCount;
    std::uint64_t octetCount;
    bool terminalCommitted;
    bool poisoned;
};

struct MediaPacketizedRtpDatagramView final {
    std::span<const std::uint8_t> bytes;
    std::size_t payloadOctets;
    MediaRunningTime presentationOnMaster;
    MediaRunningTime canonicalRelease;
};

class MediaRtpWireProtocolState;
class MediaMpegTsProtocolDatagramBatchBuffer;

class MediaRtpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaRtpWireDatagramMaterializer> create(
        MediaRtpWireDatagramMaterializerConfig config);

    ::media::Result<MediaWireDatagramBatchCollection> materialize(
        std::span<const std::uint8_t> packetizedRtp,
        std::size_t payloadOctets,
        MediaRunningTime presentationOnMaster,
        MediaRunningTime canonicalRelease);
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeBatch(std::span<const MediaPacketizedRtpDatagramView> datagrams);
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeProtocolBatch(
        std::span<const MediaPacketizedRtpDatagramView> datagrams,
        MediaMpegTsProtocolDatagramBatchBuffer& protocolBatch);

    ::media::Result<std::shared_ptr<MediaWireDatagramBatchBuffer>>
    materializeTerminalReport(
        MediaRunningTime reportInstant,
        MediaRunningTime canonicalRelease);

    ::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
    snapshot() const noexcept;

    int payloadType() const noexcept;
    int clockRate() const noexcept;
    std::uint32_t ssrc() const noexcept;
    std::uint64_t generation() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;

private:
    explicit MediaRtpWireDatagramMaterializer(
        std::shared_ptr<MediaRtpWireProtocolState> state) noexcept;
    ::media::Result<MediaWireDatagramBatchCollection>
    materializeBatchReserved(
        std::span<const MediaPacketizedRtpDatagramView> datagrams,
        MediaMpegTsProtocolDatagramBatchBuffer* protocolBatch);

    std::shared_ptr<MediaRtpWireProtocolState> m_state;
};

} // namespace media::ffmpeg::graph
