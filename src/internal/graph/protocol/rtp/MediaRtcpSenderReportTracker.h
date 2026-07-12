#pragma once

#include "internal/graph/protocol/rtp/MediaRtcpClockEvidence.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompoundParser.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace media::ffmpeg::graph {

struct MediaRtcpSenderReportTrackerConfig final {
    bool requireSenderReports;
    bool requireCname;
    int64_t senderReportTimeoutNs;
    int64_t cnameTimeoutNs;
};

class MediaRtcpSenderReportTracker final {
public:
    explicit MediaRtcpSenderReportTracker(MediaRtcpSenderReportTrackerConfig config);

    void observeMedia(uint32_t ssrc, int64_t observedAtNs);
    ::media::Status observe(const std::vector<MediaRtcpPacket>& packets, int64_t observedAtNs);
    ::media::Result<MediaRtcpClockEvidence> evidence(int64_t observedAtNs) const;
    uint64_t generation() const noexcept;
    void reset() noexcept;

private:
    void invalidate() noexcept;
    ::media::Status observeSenderReport(const MediaRtcpSenderReport& report, int64_t observedAtNs);
    ::media::Status observeSdes(const MediaRtcpSdesChunk& chunk, int64_t observedAtNs);

    MediaRtcpSenderReportTrackerConfig m_config;
    std::optional<uint32_t> m_mediaSsrc;
    std::optional<MediaRtcpSenderReport> m_senderReport;
    std::optional<MediaRtcpNtpTimestamp> m_lastAcceptedNtp;
    std::vector<uint8_t> m_cname;
    int64_t m_senderReportObservedAtNs;
    int64_t m_cnameObservedAtNs;
    uint64_t m_generation;
};

} // namespace media::ffmpeg::graph
