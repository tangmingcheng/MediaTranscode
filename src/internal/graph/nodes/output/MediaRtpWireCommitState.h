#pragma once

#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"

#include <memory>
#include <mutex>
#include <optional>
#include <deque>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaRtpWireCommitActionKind {
    SenderReport,
    Media,
    TerminalReport
};

struct MediaRtpWireCommitAction final {
    MediaRtpWireCommitActionKind kind;
    std::optional<MediaRtcpSenderReportCommitToken> reportToken;
    std::uint64_t payloadOctets;
    std::optional<MediaRtpTimestamp> timestamp;
    std::optional<MediaProtocolDatagramCommitLease> protocolCommit;
};

class MediaRtpWireCommitTransaction;

class MediaRtpWireProtocolState final {
public:
    explicit MediaRtpWireProtocolState(
        MediaRtpWireDatagramMaterializerConfig config) noexcept;

private:
    friend class MediaRtpWireDatagramMaterializer;
    friend class MediaRtpWireCommitTransaction;

    mutable std::mutex mutex;
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
    MediaRtcpSenderReportSchedule projectedSenderReportSchedule;
    std::string cname;
    std::uint16_t nextRtpSequence;
    std::uint64_t packetCount;
    std::uint64_t octetCount;
    std::optional<MediaRtpTimestamp> lastCommittedTimestamp;
    std::uint16_t projectedNextRtpSequence;
    std::uint64_t projectedPacketCount;
    std::uint64_t projectedOctetCount;
    std::optional<MediaRtpTimestamp> projectedLastTimestamp;
    std::optional<MediaRunningTime> projectedLastCanonicalRelease;
    std::size_t maximumDatagramBytes;
    std::size_t maximumOutstandingDatagrams;
    MediaDatagramBatchPlan batchPlan;
    std::size_t outstandingDatagrams = 0;
    std::uint64_t nextReservationIdentity = 1;
    struct ReservationRecord final {
        std::uint64_t identity;
        std::size_t actionCount;
        std::size_t committed;
    };
    std::deque<ReservationRecord> reservations;
    bool terminalCommitted = false;
    bool projectedTerminal = false;
    bool poisoned = false;
};

class MediaRtpWireCommitTransaction final {
public:
    MediaRtpWireCommitTransaction(
        std::shared_ptr<MediaRtpWireProtocolState> state,
        std::uint64_t reservationIdentity,
        MediaWireGlobalSequenceReservation globalReservation,
        std::vector<MediaRtpWireCommitAction> actions) noexcept;
    ~MediaRtpWireCommitTransaction() noexcept;

    ::media::Result<std::uint64_t> sequence(
        std::size_t index) const noexcept;
    ::media::Status commit(std::size_t index) noexcept;

private:
    ::media::Status poison(::media::ErrorInfo error) noexcept;

    std::shared_ptr<MediaRtpWireProtocolState> m_state;
    std::uint64_t m_reservationIdentity = 0;
    MediaWireGlobalSequenceReservation m_globalReservation;
    std::vector<MediaRtpWireCommitAction> m_actions;
    std::size_t m_nextAction = 0;
};

::media::Result<MediaDatagramSubmitCommitLease> makeMediaRtpWireCommitLease(
    const std::shared_ptr<MediaRtpWireCommitTransaction>& transaction,
    std::size_t index,
    std::uint64_t generation,
    std::uint64_t globalSequence);

} // namespace media::ffmpeg::graph
