#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>
#include <variant>
#include "internal/graph/protocol/rtp/MediaRtpSourceUnavailable.h"

namespace media::ffmpeg::graph {

struct MediaRtcpSenderReportTrackerConfig final {
    bool requireSenderReports;
    bool requireCname;
    int64_t senderReportTimeoutNs;
    int64_t cnameTimeoutNs;
};

struct MediaRtcpObservationAccepted final {};
using MediaRtcpObservation = std::variant<MediaRtcpObservationAccepted, MediaRtpClockInvalidation>;

// Scalar transaction state borrows either the validated datagram or the current
// tracker. It is materialized synchronously before either owner can change.
struct MediaRtcpTrackerState final {
    struct Pending final { MediaRtcpSenderReport report; std::span<const uint8_t> cname; int64_t observedAtNs; };
    std::optional<uint32_t> mediaSsrc;
    std::optional<Pending> pending;
    std::optional<MediaRtcpSenderReport> senderReport;
    std::optional<MediaRtcpNtpTimestamp> lastNtp;
    std::span<const uint8_t> cname;
    int64_t reportAtNs;
    int64_t cnameAtNs;
    uint64_t generation;
    bool evidencePending;
};
using MediaRtcpPreparedOutput = std::variant<std::monostate, MediaRtcpClockEvidence, MediaRtpClockInvalidation>;
class MediaRtcpSenderReportTracker;
struct MediaRtcpPreparedObservation final {
    const MediaRtcpSenderReportTracker* owner;
    uint64_t revision;
    MediaRtcpTrackerState state;
    std::optional<MediaRtcpBytes> cname;
    std::optional<MediaRtcpBytes> pendingCname;
    MediaRtcpPreparedOutput output;
    std::optional<::media::ErrorInfo> rejection;
    bool invalidated;
};

class MediaRtcpSenderReportTracker final {
public:
    explicit MediaRtcpSenderReportTracker(MediaRtcpSenderReportTrackerConfig config,
        MediaLineageAllocationScope allocation);
    ::media::Status observeMedia(uint32_t ssrc);
    ::media::Result<MediaRtcpPreparedObservation> prepare(
        const MediaRtcpPackets& packets, int64_t observedAtNs, int64_t evaluationAtNs) const;
    ::media::Result<MediaRtcpPreparedObservation> prepareEvidence(int64_t evaluationAtNs) const;
    ::media::Result<MediaRtcpPreparedObservation> prepareContinuityChange(bool invalidate) const;
    ::media::Status commit(MediaRtcpPreparedObservation&& prepared);
    uint64_t generation() const noexcept;
private:
    struct PendingRtcpSource final {
        MediaRtcpSenderReport senderReport;
        MediaRtcpBytes cname;
        int64_t observedAtNs;
    };
    MediaRtcpTrackerState state() const noexcept;
    static ::media::Status invalidatePrepared(MediaRtcpPreparedObservation& prepared);
    ::media::Result<MediaRtcpPreparedObservation> materialize(
        MediaRtcpPreparedObservation result, int64_t evaluationAtNs, bool evaluateEvidence) const;
    ::media::Status advanceRevision();
    MediaRtcpSenderReportTrackerConfig m_config;
    std::optional<uint32_t> m_mediaSsrc;
    std::optional<PendingRtcpSource> m_pendingSource;
    std::optional<MediaRtcpSenderReport> m_senderReport;
    std::optional<MediaRtcpNtpTimestamp> m_lastAcceptedNtp;
    MediaRtcpBytes m_cname;
    int64_t m_senderReportObservedAtNs;
    int64_t m_cnameObservedAtNs;
    uint64_t m_generation;
    bool m_evidenceUpdatePending = false;
    uint64_t m_revision = 0;
};

} // namespace media::ffmpeg::graph
