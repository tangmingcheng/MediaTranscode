#pragma once

#include "internal/graph/nodes/output/MediaRtpWireDatagramMaterializer.h"

#include <memory>
#include <mutex>
#include <optional>
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
    std::shared_ptr<MediaWireGlobalSequenceState> globalSequence;
    MediaRtpDatagramRewriteIdentity identity;
    MediaRtpOutputClockMapper clockMapper;
    MediaSharedNtpEpoch ntpEpoch;
    MediaRtcpSenderReportSchedule senderReportSchedule;
    std::string cname;
    std::uint16_t nextRtpSequence;
    std::uint64_t packetCount;
    std::uint64_t octetCount;
    std::optional<MediaRtpTimestamp> lastCommittedTimestamp;
    std::size_t maximumDatagramBytes;
    bool terminalCommitted = false;
    bool poisoned = false;
};

class MediaRtpWireCommitTransaction final {
public:
    MediaRtpWireCommitTransaction(
        std::shared_ptr<MediaRtpWireProtocolState> state,
        std::unique_lock<std::mutex> protocolLock,
        MediaWireGlobalSequenceReservation globalReservation,
        std::vector<MediaRtpWireCommitAction> actions) noexcept;
    ~MediaRtpWireCommitTransaction() noexcept;

    ::media::Result<std::uint64_t> sequence(
        std::size_t index) const noexcept;
    ::media::Status commit(std::size_t index) noexcept;

private:
    ::media::Status poison(::media::ErrorInfo error) noexcept;

    std::shared_ptr<MediaRtpWireProtocolState> m_state;
    std::unique_lock<std::mutex> m_protocolLock;
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
