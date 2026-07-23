#include "internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h"

#include <algorithm>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool ntpLess(const MediaRtcpNtpTimestamp& left, const MediaRtcpNtpTimestamp& right)
{
    return left.seconds < right.seconds ||
           (left.seconds == right.seconds && left.fraction < right.fraction);
}

::media::Status invalidEvidence(const char* message)
{
    return ::media::Status::failure(::media::ErrorInfo::invalidArgument(message));
}

} // namespace

MediaRtcpSenderReportTracker::MediaRtcpSenderReportTracker(MediaRtcpSenderReportTrackerConfig config)
    : m_config(config)
    , m_senderReportObservedAtNs(0)
    , m_cnameObservedAtNs(0)
    , m_generation(0)
{
}

void MediaRtcpSenderReportTracker::observeMedia(uint32_t ssrc, int64_t observedAtNs)
{
    if (m_mediaSsrc && *m_mediaSsrc != ssrc) {
        invalidate();
    }
    m_mediaSsrc = ssrc;
    if (!m_pendingSource) return;
    if (m_pendingSource->senderReport.ssrc != ssrc) {
        m_pendingSource.reset();
        return;
    }
    m_senderReport = m_pendingSource->senderReport;
    m_lastAcceptedNtp = m_senderReport->ntp;
    m_cname = std::move(m_pendingSource->cname);
    m_senderReportObservedAtNs = m_pendingSource->observedAtNs;
    m_cnameObservedAtNs = m_pendingSource->observedAtNs;
    m_pendingSource.reset();
    if (observedAtNs >= m_senderReportObservedAtNs) {
        m_evidenceUpdatePending = true;
    }
}

void MediaRtcpSenderReportTracker::observeContinuityLoss() noexcept
{
    invalidate();
}

::media::Status MediaRtcpSenderReportTracker::observe(
    const std::vector<MediaRtcpPacket>& packets, int64_t observedAtNs)
{
    if (!m_mediaSsrc) {
        return observePendingSource(packets, observedAtNs);
    }
    for (const MediaRtcpPacket& packet : packets) {
        if (packet.kind == MediaRtcpPacketKind::SenderReport && packet.senderReport) {
            if (auto status = observeSenderReport(*packet.senderReport, observedAtNs); !status) return status;
        } else if (packet.kind == MediaRtcpPacketKind::SourceDescription) {
            for (const MediaRtcpSdesChunk& chunk : packet.sdesChunks) {
                if (auto status = observeSdes(chunk, observedAtNs); !status) return status;
            }
        } else if (packet.kind == MediaRtcpPacketKind::Bye && m_mediaSsrc &&
                   std::find(packet.byeSources.begin(), packet.byeSources.end(), *m_mediaSsrc) !=
                       packet.byeSources.end()) {
            invalidate();
            return ::media::Status::failure(::media::ErrorInfo::cancelled("RTCP BYE ended active source"));
        }
    }
    return ::media::Status::success();
}

::media::Result<MediaRtcpClockEvidence> MediaRtcpSenderReportTracker::evidence(
    int64_t observedAtNs) const
{
    auto missing = [](const char* message) {
        return ::media::Result<MediaRtcpClockEvidence>::failure(
            ::media::ErrorInfo::notInitialized(message));
    };
    if (m_config.senderReportTimeoutNs <= 0 || m_config.cnameTimeoutNs <= 0) {
        return ::media::Result<MediaRtcpClockEvidence>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP evidence timeouts must be positive"));
    }
    if (!m_mediaSsrc) return missing("RTP media SSRC has not been observed");
    if (m_config.requireSenderReports) {
        if (!m_senderReport || m_senderReport->ssrc != *m_mediaSsrc) {
            return missing("Matching RTCP sender report is unavailable");
        }
        if (observedAtNs < m_senderReportObservedAtNs ||
            observedAtNs - m_senderReportObservedAtNs > m_config.senderReportTimeoutNs) {
            return missing("RTCP sender report has expired");
        }
    }
    if (m_config.requireCname) {
        if (m_cname.empty()) return missing("Matching RTCP CNAME is unavailable");
        if (observedAtNs < m_cnameObservedAtNs ||
            observedAtNs - m_cnameObservedAtNs > m_config.cnameTimeoutNs) {
            return missing("RTCP CNAME has expired");
        }
    }
    if (!m_senderReport) return missing("Clock evidence requires an RTCP sender report");
    return ::media::Result<MediaRtcpClockEvidence>::success(MediaRtcpClockEvidence{
        *m_mediaSsrc, m_senderReport->ssrc, *m_mediaSsrc,
        m_senderReport->ntp, m_senderReport->rtpTimestamp, m_cname,
        m_senderReportObservedAtNs, m_cnameObservedAtNs, m_generation});
}

::media::Result<std::optional<MediaRtcpClockEvidence>>
MediaRtcpSenderReportTracker::takeEvidenceUpdate(int64_t observedAtNs)
{
    if (!m_evidenceUpdatePending) {
        return ::media::Result<std::optional<MediaRtcpClockEvidence>>::success(
            std::nullopt);
    }
    auto current = evidence(observedAtNs);
    if (!current) {
        if (current.error().code == ::media::ErrorCode::NotInitialized) {
            return ::media::Result<std::optional<MediaRtcpClockEvidence>>::success(
                std::nullopt);
        }
        return ::media::Result<std::optional<MediaRtcpClockEvidence>>::failure(
            current.error());
    }
    m_evidenceUpdatePending = false;
    return ::media::Result<std::optional<MediaRtcpClockEvidence>>::success(
        std::move(current).value());
}

uint64_t MediaRtcpSenderReportTracker::generation() const noexcept
{
    return m_generation;
}

void MediaRtcpSenderReportTracker::reset() noexcept
{
    invalidate();
}

void MediaRtcpSenderReportTracker::invalidate() noexcept
{
    ++m_generation;
    m_mediaSsrc.reset();
    m_pendingSource.reset();
    m_senderReport.reset();
    m_cname.clear();
    m_lastAcceptedNtp.reset();
    m_senderReportObservedAtNs = 0;
    m_cnameObservedAtNs = 0;
    m_evidenceUpdatePending = false;
}

::media::Status MediaRtcpSenderReportTracker::observePendingSource(
    const std::vector<MediaRtcpPacket>& packets,
    int64_t observedAtNs)
{
    const MediaRtcpSenderReport* report = nullptr;
    for (const MediaRtcpPacket& packet : packets) {
        if (packet.kind == MediaRtcpPacketKind::SenderReport &&
            packet.senderReport) {
            report = &*packet.senderReport;
            break;
        }
    }
    if (!report) return ::media::Status::success();

    const std::vector<uint8_t>* cname = nullptr;
    for (const MediaRtcpPacket& packet : packets) {
        if (packet.kind != MediaRtcpPacketKind::SourceDescription) continue;
        for (const MediaRtcpSdesChunk& chunk : packet.sdesChunks) {
            if (chunk.ssrc != report->ssrc) continue;
            const auto item = std::find_if(
                chunk.items.begin(), chunk.items.end(),
                [](const MediaRtcpSdesItem& value) { return value.type == 1; });
            if (item == chunk.items.end()) continue;
            if (item->value.empty()) {
                invalidate();
                return invalidEvidence("RTCP CNAME must not be empty");
            }
            cname = &item->value;
            break;
        }
        if (cname) break;
    }
    if (!cname) return ::media::Status::success();
    if (m_pendingSource && m_pendingSource->senderReport.ssrc == report->ssrc &&
        ntpLess(report->ntp, m_pendingSource->senderReport.ntp)) {
        invalidate();
        return invalidEvidence("Pending RTCP sender report NTP timestamp regressed");
    }
    if (m_pendingSource && m_pendingSource->senderReport.ssrc == report->ssrc &&
        m_pendingSource->senderReport.ntp == report->ntp &&
        m_pendingSource->senderReport.rtpTimestamp != report->rtpTimestamp) {
        invalidate();
        return invalidEvidence(
            "Pending RTCP sender report changed RTP timestamp at the same NTP instant");
    }
    m_pendingSource = PendingRtcpSource{*report, *cname, observedAtNs};
    return ::media::Status::success();
}

::media::Status MediaRtcpSenderReportTracker::observeSenderReport(
    const MediaRtcpSenderReport& report, int64_t observedAtNs)
{
    if (m_mediaSsrc && report.ssrc != *m_mediaSsrc) return ::media::Status::success();
    if (m_lastAcceptedNtp && ntpLess(report.ntp, *m_lastAcceptedNtp)) {
        invalidate();
        return invalidEvidence("RTCP sender report NTP timestamp regressed");
    }
    if (m_senderReport && m_senderReport->ssrc == report.ssrc &&
        m_senderReport->ntp == report.ntp && m_senderReport->rtpTimestamp != report.rtpTimestamp) {
        invalidate();
        return invalidEvidence("RTCP sender report changed RTP timestamp at the same NTP instant");
    }
    m_senderReport = report;
    m_lastAcceptedNtp = report.ntp;
    m_senderReportObservedAtNs = observedAtNs;
    m_evidenceUpdatePending = true;
    return ::media::Status::success();
}

::media::Status MediaRtcpSenderReportTracker::observeSdes(
    const MediaRtcpSdesChunk& chunk, int64_t observedAtNs)
{
    const auto cname = std::find_if(chunk.items.begin(), chunk.items.end(), [](const MediaRtcpSdesItem& item) {
        return item.type == 1;
    });
    if (cname == chunk.items.end()) return ::media::Status::success();
    if (!m_mediaSsrc || chunk.ssrc != *m_mediaSsrc) return ::media::Status::success();
    if (cname->value.empty()) {
        invalidate();
        return invalidEvidence("RTCP CNAME must not be empty");
    }
    if (!m_cname.empty() && m_cname != cname->value) {
        invalidate();
        return invalidEvidence("RTCP CNAME changed for active source");
    }
    m_cname = cname->value;
    m_cnameObservedAtNs = observedAtNs;
    m_evidenceUpdatePending = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
