#pragma once

#include "internal/graph/diagnostics/MediaDiagnosticMasterClock.h"
#include "internal/graph/runtime/buffer/MediaWireDatagramDescriptor.h"

namespace media::ffmpeg::graph {

inline void appendMediaWireProtocolDeadlineDiagnostic(std::ostream& out,
    const std::optional<MediaWireProtocolDeadlineObservation>& observation)
{
    if (!observation) return;
    const auto& deadline = *observation;
    out << " publication_role=" << (deadline.protectsMediaPublication ? "leading_report" : "media");
    appendMediaDiagnosticMasterTime(out, deadline.protectsMediaPublication
        ? "protected_media_original_deadline" : "original_protocol_deadline", deadline.originalDeadline);
    appendMediaDiagnosticMasterTime(out, "prefix_completion_limit", deadline.prefixCompletionLimit);
    appendMediaDiagnosticMasterTime(out, "materialized", deadline.materializedAt);
    if (!deadline.publication) return;
    const auto& publication = *deadline.publication;
    const auto& accepted = publication.acceptedWork;
    out << " planning_generation=" << accepted.generation
        << " planning_cursor=" << publication.cursorIdentity
        << " planning_cursor_revision=" << publication.cursorRevision
        << " planning_publication=" << publication.publicationIdentity
        << " planning_protocol_begin=" << publication.protocolBegin
        << " planning_protocol_end=" << publication.protocolEnd
        << " planning_committed_sequence=";
    if (accepted.lastCommittedSequence) out << *accepted.lastCommittedSequence;
    else out << "unavailable";
    appendMediaDiagnosticMasterTime(out, "planning_observed", accepted.observedNow);
    appendMediaDiagnosticMasterTime(out, "planning_projected_available", accepted.projectedAvailableAt);
    appendMediaDiagnosticMasterTime(out, "mux_publication_ready", publication.muxPublicationReadyAt);
    out << " planning_submitted_service=" << (accepted.submittedService ? "available" : "absent");
    if (accepted.submittedService) {
        const auto& service = *accepted.submittedService;
        out << " planning_submitted_generation=" << service.generation
            << " planning_submitted_sequence=" << service.globalSequence;
        appendMediaDiagnosticMasterTime(out, "planning_submit_completed", service.submittedAt);
        out << " planning_service_duration_ns=" << service.serviceDuration.nanoseconds();
        appendMediaDiagnosticMasterTime(out, "planning_scope_available", service.scopeAvailableAt);
    }
}

} // namespace media::ffmpeg::graph
