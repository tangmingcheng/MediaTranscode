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
    bool consumesProtocolEntry = false;
};

class MediaRtpWireCommitTransaction;

class MediaRtpWireProtocolState final {
public:
    explicit MediaRtpWireProtocolState(
        MediaRtpWireDatagramMaterializerConfig config) noexcept;

private:
    friend class MediaRtpWireDatagramMaterializer;
    friend class MediaRtpWireCommitTransaction;

    bool canAdmitReservation(std::size_t datagrams) const noexcept;

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
    std::weak_ptr<MediaNodeWakeup> reservationWakeup;
    std::optional<std::size_t> blockedReservationDatagrams;
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
        std::vector<MediaRtpWireCommitAction> actions,
        std::optional<MediaProtocolDatagramCommitTransaction>
            protocolCommit) noexcept;
    MediaRtpWireCommitTransaction(
        MediaRtpWireCommitTransaction&&) noexcept = default;
    MediaRtpWireCommitTransaction& operator=(
        MediaRtpWireCommitTransaction&&) = delete;
    ~MediaRtpWireCommitTransaction() noexcept;

    std::size_t size() const noexcept { return m_actions.size(); }
    ::media::Result<std::uint64_t> sequence(
        std::size_t index) const noexcept;
    ::media::Status markScheduledPrefix(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;
    ::media::Status commitSubmittedPrefix(
        std::size_t begin,
        std::size_t count,
        MediaRunningTime now) noexcept;

private:
    ::media::Status poison(::media::ErrorInfo error) noexcept;
    ::media::Status poisonLocked(::media::ErrorInfo error) noexcept;

    std::shared_ptr<MediaRtpWireProtocolState> m_state;
    std::uint64_t m_reservationIdentity = 0;
    MediaWireGlobalSequenceReservation m_globalReservation;
    std::vector<MediaRtpWireCommitAction> m_actions;
    std::optional<MediaProtocolDatagramCommitTransaction> m_protocolCommit;
    std::size_t m_nextAction = 0;
};

} // namespace media::ffmpeg::graph
