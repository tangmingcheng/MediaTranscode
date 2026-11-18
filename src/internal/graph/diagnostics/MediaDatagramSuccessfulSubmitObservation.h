#pragma once

#include "internal/graph/diagnostics/MediaDatagramReservationObservation.h"
#include "internal/graph/runtime/network/MediaDatagramPacingController.h"
#include <type_traits>

namespace media::ffmpeg::graph {

// One owner-thread value snapshot selected by pacing telemetry, not a second
// lateness calculation. No media ownership or per-submit log formatting.
struct MediaDatagramSuccessfulSubmitObservation final {
    std::uint64_t generation;
    std::uint64_t endpoint;
    std::uint64_t sequence;
    std::int64_t pacingLatenessNs;
    MediaRunningTime release;
    MediaRunningTime deadline;
    std::optional<MediaWireProtocolDeadlineObservation> protocolDeadline;
    MediaDatagramPacingReservation pacing;
    MediaRunningTime reservedAt;
    MediaRunningTime submitStartedAt;
    MediaRunningTime submitCompletedAt;
    MediaDatagramReservationObservation reservation;

    void log(const MediaDatagramPacingTelemetry& telemetry) const noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        try {
            std::ostringstream out;
            out << "datagram_successful_submit_trace stage=worst_success generation=" << generation
                << " endpoint=" << endpoint << " sequence=" << sequence
                << " pacing_lateness_ns=" << pacingLatenessNs
                << " matches_pacing_worst=" << (telemetry.worstLateGlobalSequence == sequence &&
                    telemetry.maximumSubmitLatenessNanoseconds == pacingLatenessNs)
                << " enqueued_master_ns=unavailable"
                << " submit_interval_scope=pre_transport_checks_and_transport"
                << " socket_only_ns=unavailable";
            appendMediaDiagnosticMasterTime(out, "release", release);
            appendMediaDiagnosticMasterTime(out, "deadline", deadline);
            if (!protocolDeadline)
                appendMediaDiagnosticMasterTime(out, "materialized", std::nullopt);
            appendMediaWireProtocolDeadlineDiagnostic(out, protocolDeadline);
            appendMediaDiagnosticMasterTime(out, "reserved_at", reservedAt);
            appendMediaDiagnosticMasterTime(out, "not_before", pacing.notBefore);
            appendMediaDiagnosticMasterTime(out, "not_after", pacing.notAfter);
            out << " service_duration_ns=" << pacing.serviceDuration.nanoseconds()
                << " wire_bytes_per_second=" << pacing.wireBytesPerSecond;
            appendMediaDiagnosticMasterTime(out, "submit_started", submitStartedAt);
            appendMediaDiagnosticMasterTime(out, "submit_completed", submitCompletedAt);
            reservation.append(out);
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {
            // Optional diagnostics must not change the production result.
        }
    }
};
static_assert(std::is_trivially_copyable_v<MediaDatagramSuccessfulSubmitObservation>);

} // namespace media::ffmpeg::graph
