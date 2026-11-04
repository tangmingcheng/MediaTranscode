#pragma once
#include "internal/graph/nodes/output/MediaRtpWireInputBatch.h"

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

class MediaNodeWakeup;

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
    std::shared_ptr<MediaNodeWakeup> reservationWakeup;
};

struct MediaRtpWireDatagramMaterializerSnapshot final {
    std::uint16_t nextRtpSequence;
    std::uint64_t packetCount;
    std::uint64_t octetCount;
    bool terminalCommitted;
    bool poisoned;
};

class MediaRtpWireProtocolState;
class MediaRtpWirePreparedTransaction;
class MediaMpegTsProtocolDatagramBatchBuffer;

class MediaRtpWireDatagramMaterializer final {
public:
    static ::media::Result<MediaRtpWireDatagramMaterializer> create(
        MediaRtpWireDatagramMaterializerConfig config);

    ::media::Result<MediaRtpWirePreparedTransaction> prepareBatch(
        MediaRtpWireInputBatch datagrams,
        std::shared_ptr<MediaMpegTsProtocolDatagramBatchBuffer> protocolBatch,
        MediaRunningTime materializedAt,
        MediaNetworkStorageLease* prepaid);
    ::media::Result<MediaWireDatagramBatchCollection> complete(
        MediaRtpWirePreparedTransaction& pending, MediaRunningTime now);

    // An empty successful result means no RTP/RTCP participation: no BYE
    // is permitted (RFC 3550 section 6.3.7).
    ::media::Result<std::optional<MediaRtpWirePreparedTransaction>>
    prepareTerminalReport(
        MediaRunningTime reportInstant,
        MediaRunningTime canonicalRelease,
        MediaRunningTime materializedAt);

    ::media::Result<MediaRtpWireDatagramMaterializerSnapshot>
    snapshot() const noexcept;

    int payloadType() const noexcept;
    int clockRate() const noexcept;
    std::uint32_t ssrc() const noexcept;
    std::uint64_t generation() const noexcept;
    std::size_t maximumDatagramBytes() const noexcept;
    std::size_t maximumBatchDatagrams() const noexcept;

private:
    explicit MediaRtpWireDatagramMaterializer(
        std::shared_ptr<MediaRtpWireProtocolState> state) noexcept;
    std::shared_ptr<MediaRtpWireProtocolState> m_state;
};

} // namespace media::ffmpeg::graph
