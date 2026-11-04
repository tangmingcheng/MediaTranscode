#pragma once

#include "internal/graph/diagnostics/MediaDiagnosticMasterClock.h"
#include "internal/graph/diagnostics/MediaDatagramWaitObservation.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"

#include <sstream>
#include <string_view>

namespace media::ffmpeg::graph {

// Owner-thread observations, never inputs to admission or scheduling.
struct MediaDatagramReservationObservation final {
    std::optional<MediaRunningTime> activationStartedAt;
    std::optional<MediaRunningTime> activationCompletedAt;
    std::optional<MediaRunningTime> permitStartedAt;
    std::optional<MediaRunningTime> permitCompletedAt;
    std::optional<MediaRunningTime> snapshotStartedAt;
    std::optional<MediaRunningTime> snapshotSampledAt;
    std::optional<MediaRunningTime> snapshotCompletedAt;
    std::optional<MediaRunningTime> reservationCompletedAt;

    std::optional<MediaRunningTime> waitStartedAt;
    std::optional<MediaRunningTime> waitCompletedAt;
    std::optional<MediaRunningTime> submitObservedAt;
    MediaDatagramWaitObservation waits;

    void append(std::ostream& out) const
    {
        appendMediaDiagnosticMasterTime(out, "activation_started", activationStartedAt);
        appendMediaDiagnosticMasterTime(out, "activation_completed", activationCompletedAt);
        appendMediaDiagnosticMasterTime(out, "permit_started", permitStartedAt);
        appendMediaDiagnosticMasterTime(out, "permit_completed", permitCompletedAt);
        appendMediaDiagnosticMasterTime(out, "snapshot_started", snapshotStartedAt);
        appendMediaDiagnosticMasterTime(out, "snapshot_sampled", snapshotSampledAt);
        appendMediaDiagnosticMasterTime(out, "snapshot_completed", snapshotCompletedAt);
        appendMediaDiagnosticMasterTime(out, "reservation_completed", reservationCompletedAt);
        appendMediaDiagnosticMasterTime(out, "wait_started", waitStartedAt);
        appendMediaDiagnosticMasterTime(out, "wait_completed", waitCompletedAt);
        appendMediaDiagnosticMasterTime(out, "submit_observed", submitObservedAt);
        waits.append(out);
    }

    void logFailure(std::string_view stage, std::uint64_t generation, std::uint64_t endpoint,
        std::uint64_t sequence, MediaRunningTime release,
        MediaRunningTime deadline,
        const std::optional<MediaWireProtocolDeadlineObservation>& protocolDeadline,
        const ::media::ErrorInfo& error) const noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        try {
            std::ostringstream out;
            out << "datagram_reservation_trace stage=" << stage << " generation=" << generation
                << " endpoint=" << endpoint << " sequence=" << sequence;
            appendMediaDiagnosticMasterTime(out, "release", release);
            appendMediaDiagnosticMasterTime(out, "deadline", deadline);
            if (protocolDeadline) {
                appendMediaDiagnosticMasterTime(out, "original_protocol_deadline", protocolDeadline->originalDeadline);
                appendMediaDiagnosticMasterTime(out, "prefix_completion_limit", protocolDeadline->prefixCompletionLimit);
            }
            append(out);
            out << " original_error=\"" << error.describe() << '\"';
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {
            // Diagnostics must preserve the original production failure.
        }
    }
};

} // namespace media::ffmpeg::graph
