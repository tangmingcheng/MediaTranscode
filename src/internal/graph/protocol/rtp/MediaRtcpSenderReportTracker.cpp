#include "internal/graph/protocol/rtp/MediaRtcpSenderReportTracker.h"

#include <algorithm>
#include <utility>
#include <limits>

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

::media::Result<MediaRtcpClockEvidence> evidenceForState(
    const MediaRtcpTrackerState& state, const MediaRtcpSenderReportTrackerConfig& config,
    const MediaRtcpBytes::Allocator& allocator, int64_t observedAtNs)
{
    auto missing = [](const char* message) {
        return ::media::Result<MediaRtcpClockEvidence>::failure(
            ::media::ErrorInfo::notInitialized(message));
    };
    if (config.senderReportTimeoutNs <= 0 || config.cnameTimeoutNs <= 0) {
        return ::media::Result<MediaRtcpClockEvidence>::failure(
            ::media::ErrorInfo::invalidArgument("RTCP evidence timeouts must be positive"));
    }
    if (!state.mediaSsrc) return missing("RTP media SSRC has not been observed");
    if (config.requireSenderReports) {
        if (!state.senderReport || state.senderReport->ssrc != *state.mediaSsrc) {
            return missing("Matching RTCP sender report is unavailable");
        }
        if (observedAtNs < state.reportAtNs ||
            observedAtNs - state.reportAtNs > config.senderReportTimeoutNs) {
            return missing("RTCP sender report has expired");
        }
    }
    if (config.requireCname) {
        if (state.cname.empty()) return missing("Matching RTCP CNAME is unavailable");
        if (observedAtNs < state.cnameAtNs ||
            observedAtNs - state.cnameAtNs > config.cnameTimeoutNs) {
            return missing("RTCP CNAME has expired");
        }
    }
    if (!state.senderReport) return missing("Clock evidence requires an RTCP sender report");
    return ::media::Result<MediaRtcpClockEvidence>::success(MediaRtcpClockEvidence{
        *state.mediaSsrc, state.senderReport->ssrc, *state.mediaSsrc,
        state.senderReport->ntp, state.senderReport->rtpTimestamp, MediaRtcpBytes{state.cname, allocator},
        state.reportAtNs, state.cnameAtNs, state.generation});
}


} // namespace

MediaRtcpSenderReportTracker::MediaRtcpSenderReportTracker(MediaRtcpSenderReportTrackerConfig config,
    MediaLineageAllocationScope allocation)
    : m_config(config)
    , m_cname(MediaLineageAllocator<uint8_t>{std::move(allocation)})
    , m_senderReportObservedAtNs(0)
    , m_cnameObservedAtNs(0)
    , m_generation(0)
{
}

::media::Status MediaRtcpSenderReportTracker::observeMedia(uint32_t ssrc)
{
    if (m_mediaSsrc && *m_mediaSsrc != ssrc) {
        auto prepared = prepareContinuityChange(true);
        if (!prepared) return ::media::Status::failure(prepared.error());
        if (auto status = commit(std::move(prepared).value()); !status) return status;
    } else if (auto status = advanceRevision(); !status) return status;
    m_mediaSsrc = ssrc;
    if (!m_pendingSource) return ::media::Status::success();
    if (m_pendingSource->senderReport.ssrc != ssrc) {
        m_pendingSource.reset();
        return ::media::Status::success();
    }
    m_senderReport = m_pendingSource->senderReport;
    m_lastAcceptedNtp = m_senderReport->ntp;
    m_cname = std::move(m_pendingSource->cname);
    m_senderReportObservedAtNs = m_pendingSource->observedAtNs;
    m_cnameObservedAtNs = m_pendingSource->observedAtNs;
    m_pendingSource.reset();
    // RTP and RTCP can be delivered by independent receivers. Matching the
    // media SSRC completes pending evidence regardless of cross-channel arrival
    // order; prepareEvidence evaluates age against its supplied clock.
    m_evidenceUpdatePending = true;
    return ::media::Status::success();
}

uint64_t MediaRtcpSenderReportTracker::generation() const noexcept
{
    return m_generation;
}

::media::Status MediaRtcpSenderReportTracker::advanceRevision()
{
    if (m_revision == std::numeric_limits<uint64_t>::max())
        return invalidEvidence("RTCP tracker mutation revision exhausted");
    ++m_revision;
    return ::media::Status::success();
}

MediaRtcpTrackerState MediaRtcpSenderReportTracker::state() const noexcept
{
    MediaRtcpTrackerState result{m_mediaSsrc, std::nullopt, m_senderReport, m_lastAcceptedNtp,
        m_cname.view(), m_senderReportObservedAtNs, m_cnameObservedAtNs, m_generation, m_evidenceUpdatePending};
    if (m_pendingSource) result.pending.emplace(MediaRtcpTrackerState::Pending{
        m_pendingSource->senderReport, m_pendingSource->cname.view(), m_pendingSource->observedAtNs});
    return result;
}

::media::Result<MediaRtcpPreparedObservation> MediaRtcpSenderReportTracker::prepare(
    const MediaRtcpPackets& packets, int64_t observedAtNs, int64_t evaluationAtNs) const try
{
    using Result = ::media::Result<MediaRtcpPreparedObservation>;
    if (m_revision == std::numeric_limits<uint64_t>::max())
        return Result::failure(::media::ErrorInfo::invalidArgument("RTCP tracker mutation revision exhausted"));
    MediaRtcpPreparedObservation result{this, m_revision, state(), {}, {}, {}, {}, false, {}};
    auto& next = result.state;
    auto reject = [&](const char* message) -> ::media::Status {
        auto status = invalidatePrepared(result);
        if (!status) return status;
        result.rejection = ::media::ErrorInfo::invalidArgument(message);
        return ::media::Status::success();
    };
    if (!next.mediaSsrc) {
        std::optional<MediaRtcpSenderReport> report;
        for (const auto packet : packets) if (packet.kind == MediaRtcpPacketKind::SenderReport && packet.senderReport) {
            report = packet.senderReport; break;
        }
        if (report) {
            std::optional<std::span<const uint8_t>> cname;
            for (const auto packet : packets) {
                if (packet.kind != MediaRtcpPacketKind::SourceDescription) continue;
                for (const auto chunk : packet.sdesChunks) {
                    if (chunk.ssrc != report->ssrc) continue;
                    const auto item = std::find_if(chunk.items.begin(), chunk.items.end(),
                        [](const auto& value) { return value.type == 1; });
                    if (item == chunk.items.end()) continue;
                    if ((*item).value.empty()) {
                        if (auto status = reject("RTCP CNAME must not be empty"); !status) return Result::failure(status.error());
                        break;
                    }
                    cname = (*item).value; break;
                }
                if (cname || result.rejection) break;
            }
            if (!result.rejection && (cname || !m_config.requireCname)) {
                if (next.pending && next.pending->report.ssrc == report->ssrc &&
                    ntpLess(report->ntp, next.pending->report.ntp)) {
                    if (auto status = reject("Pending RTCP sender report NTP timestamp regressed"); !status) return Result::failure(status.error());
                } else if (next.pending && next.pending->report.ssrc == report->ssrc &&
                    next.pending->report.ntp == report->ntp && next.pending->report.rtpTimestamp != report->rtpTimestamp) {
                    if (auto status = reject("Pending RTCP sender report changed RTP timestamp at the same NTP instant"); !status) return Result::failure(status.error());
                } else next.pending.emplace(MediaRtcpTrackerState::Pending{*report, cname.value_or(std::span<const uint8_t>{}), observedAtNs});
            }
        }
    } else {
        for (const auto packet : packets) {
            if (packet.kind == MediaRtcpPacketKind::SenderReport && packet.senderReport) {
                const auto& report = *packet.senderReport;
                if (report.ssrc != *next.mediaSsrc) continue;
                if (next.lastNtp && ntpLess(report.ntp, *next.lastNtp)) {
                    if (auto status = reject("RTCP sender report NTP timestamp regressed"); !status) return Result::failure(status.error());
                } else if (next.senderReport && next.senderReport->ssrc == report.ssrc &&
                    next.senderReport->ntp == report.ntp && next.senderReport->rtpTimestamp != report.rtpTimestamp) {
                    if (auto status = reject("RTCP sender report changed RTP timestamp at the same NTP instant"); !status) return Result::failure(status.error());
                } else {
                    next.senderReport = report; next.lastNtp = report.ntp; next.reportAtNs = observedAtNs;
                    if (!m_config.requireCname) next.cnameAtNs = observedAtNs;
                    next.evidencePending = true;
                }
            } else if (packet.kind == MediaRtcpPacketKind::SourceDescription) {
                for (const auto chunk : packet.sdesChunks) {
                    const auto item = std::find_if(chunk.items.begin(), chunk.items.end(), [](const auto& value) { return value.type == 1; });
                    if (item == chunk.items.end() || chunk.ssrc != *next.mediaSsrc) continue;
                    if ((*item).value.empty()) {
                        if (auto status = reject("RTCP CNAME must not be empty"); !status) return Result::failure(status.error());
                    } else if (!next.cname.empty() && !std::ranges::equal(next.cname, (*item).value)) {
                        if (auto status = reject("RTCP CNAME changed for active source"); !status) return Result::failure(status.error());
                    } else { next.cname = (*item).value; next.cnameAtNs = observedAtNs; next.evidencePending = true; }
                    if (result.rejection) break;
                }
            } else if (packet.kind == MediaRtcpPacketKind::Bye &&
                std::find(packet.byeSources.begin(), packet.byeSources.end(), *next.mediaSsrc) != packet.byeSources.end()) {
                const MediaRtpSenderDeparture departure{next.generation, *next.mediaSsrc, observedAtNs};
                if (auto status = invalidatePrepared(result); !status) return Result::failure(status.error());
                result.senderDeparture = departure;
                result.output = MediaRtpClockInvalidation{next.generation, MediaRtpSourceUnavailableReason::SenderLeft, observedAtNs};
                break;
            }
            if (result.rejection) break;
        }
    }
    return materialize(std::move(result), evaluationAtNs, true);
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtcpPreparedObservation>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock observation allocation failed"));
}

::media::Result<MediaRtcpPreparedObservation> MediaRtcpSenderReportTracker::prepareEvidence(int64_t now) const
{
    return materialize({this, m_revision, state(), {}, {}, {}, {}, false, {}}, now, true);
}

::media::Status MediaRtcpSenderReportTracker::invalidatePrepared(MediaRtcpPreparedObservation& result)
{
    if (result.state.generation == std::numeric_limits<uint64_t>::max())
        return invalidEvidence("RTCP tracker generation exhausted");
    const auto generation = result.state.generation + 1;
    result.state = {}; result.state.generation = generation; result.invalidated = true;
    return ::media::Status::success();
}

::media::Result<MediaRtcpPreparedObservation> MediaRtcpSenderReportTracker::prepareContinuityChange(bool invalidate) const
{
    using Result = ::media::Result<MediaRtcpPreparedObservation>;
    MediaRtcpPreparedObservation result{this, m_revision, state(), {}, {}, {}, {}, false, {}};
    if (invalidate) {
        if (auto status = invalidatePrepared(result); !status) return Result::failure(status.error());
    }
    // This delta has no evidence evaluation or wire capture time.
    return materialize(std::move(result), 0, false);
}

::media::Result<MediaRtcpPreparedObservation> MediaRtcpSenderReportTracker::prepareMemberRetirement(
    uint64_t expectedIngressGeneration, uint32_t expectedSsrc,
    MediaRtpSourceUnavailableReason reason, int64_t observedAtNs) const
{
    using Result = ::media::Result<MediaRtcpPreparedObservation>;
    if (!m_mediaSsrc || *m_mediaSsrc != expectedSsrc || m_generation != expectedIngressGeneration ||
        observedAtNs <= 0 || (reason != MediaRtpSourceUnavailableReason::SenderLeft &&
            reason != MediaRtpSourceUnavailableReason::GroupClosed))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "RTP member retirement requires its exact frozen member and closing reason"));
    auto prepared = prepareContinuityChange(true);
    if (!prepared) return prepared;
    prepared.value().output = MediaRtpClockInvalidation{prepared.value().state.generation, reason, observedAtNs};
    if (reason == MediaRtpSourceUnavailableReason::SenderLeft)
        prepared.value().senderDeparture = MediaRtpSenderDeparture{expectedIngressGeneration, expectedSsrc, observedAtNs};
    return prepared;
}

::media::Result<MediaRtcpPreparedObservation> MediaRtcpSenderReportTracker::materialize(
    MediaRtcpPreparedObservation result, int64_t evaluationAtNs, bool evaluateEvidence) const try
{
    using Result = ::media::Result<MediaRtcpPreparedObservation>;
    if (result.owner != this || result.revision != m_revision || m_revision == std::numeric_limits<uint64_t>::max())
        return Result::failure(::media::ErrorInfo::invalidArgument("RTCP tracker mutation revision exhausted or stale"));
    auto& next = result.state;
    if (evaluateEvidence && !result.invalidated && next.evidencePending) {
        auto evidence = evidenceForState(next, m_config, m_cname.get_allocator(), evaluationAtNs);
        if (evidence) {
            result.output = std::move(evidence).value();
            next.evidencePending = false;
        } else if (evidence.error().code != ::media::ErrorCode::NotInitialized)
            result.rejection = evidence.error();
    }
    if (next.cname.data() != m_cname.view().data() || next.cname.size() != m_cname.size())
        result.cname.emplace(next.cname, m_cname.get_allocator());
    if (next.pending && (!m_pendingSource || next.pending->cname.data() != m_pendingSource->cname.view().data() ||
        next.pending->cname.size() != m_pendingSource->cname.size()))
        result.pendingCname.emplace(next.pending->cname, m_cname.get_allocator());
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<MediaRtcpPreparedObservation>::failure(
        ::media::ErrorInfo::allocationFailed("RTP clock observation allocation failed"));
}

::media::Status MediaRtcpSenderReportTracker::commit(MediaRtcpPreparedObservation&& prepared)
{
    if (prepared.owner != this || prepared.revision != m_revision)
        return invalidEvidence("RTCP prepared observation no longer matches tracker revision");
    const auto* invalidation = std::get_if<MediaRtpClockInvalidation>(&prepared.output);
    const bool senderLeft = invalidation && invalidation->reason == MediaRtpSourceUnavailableReason::SenderLeft;
    if (senderLeft != prepared.senderDeparture.has_value())
        return invalidEvidence("RTCP sender departure must accompany its exact BYE invalidation");
    if (prepared.senderDeparture) {
        const auto& departure = *prepared.senderDeparture;
        if (!prepared.invalidated || !m_mediaSsrc || departure.ssrc != *m_mediaSsrc ||
            departure.ingressGeneration != m_generation ||
            m_generation == std::numeric_limits<uint64_t>::max() ||
            prepared.state.generation != m_generation + 1 ||
            invalidation->generation != prepared.state.generation ||
            departure.observedAtNs != invalidation->observedAtNs)
            return invalidEvidence("RTCP sender departure changed its pre-invalidation member identity");
    }
    if (auto status = advanceRevision(); !status) return status;
    const auto& next = prepared.state;
    m_mediaSsrc = next.mediaSsrc; m_senderReport = next.senderReport; m_lastAcceptedNtp = next.lastNtp;
    m_senderReportObservedAtNs = next.reportAtNs; m_cnameObservedAtNs = next.cnameAtNs;
    m_generation = next.generation; m_evidenceUpdatePending = next.evidencePending;
    if (prepared.cname) m_cname = std::move(*prepared.cname);
    if (!next.pending) m_pendingSource.reset();
    else if (prepared.pendingCname) m_pendingSource.emplace(PendingRtcpSource{
        next.pending->report, std::move(*prepared.pendingCname), next.pending->observedAtNs});
    else { m_pendingSource->senderReport = next.pending->report; m_pendingSource->observedAtNs = next.pending->observedAtNs; }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
