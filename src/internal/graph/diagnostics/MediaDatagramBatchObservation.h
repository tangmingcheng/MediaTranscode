#pragma once

#include "internal/graph/diagnostics/MediaDatagramReservationObservation.h"

namespace media::ffmpeg::graph {

// Fixed owner-thread diagnostics. No packet retention and no scheduling input.
struct MediaDatagramBatchObservation final {
    std::uint64_t generation = 0;
    std::uint64_t firstSequence = 0;
    std::uint64_t lastSequence = 0;
    std::optional<MediaRunningTime> firstRelease;
    std::optional<MediaRunningTime> firstDeadline;
    std::optional<MediaRunningTime> lastRelease;
    std::optional<MediaRunningTime> lastDeadline;
    std::optional<MediaRunningTime> firstSubmitted;
    std::optional<MediaRunningTime> lastSubmitted;
    std::int64_t maximumSubmitIntervalNs = 0;
    std::uint64_t maximumSubmitIntervalSequence = 0;
    std::int64_t maximumStartLatenessNs = 0;
    std::uint64_t maximumStartLatenessSequence = 0;
    MediaDatagramReservationObservation worstStartObservation;
    MediaDatagramWaitObservation waits;

    void observe(std::uint64_t sequence, MediaRunningTime notBefore,
        MediaRunningTime started, MediaRunningTime completed,
        const MediaDatagramReservationObservation& observation) noexcept
    {
        if (!firstSubmitted) firstSubmitted = completed;
        lastSubmitted = completed;
        // Samples include the scope/publication checks before the transport call.
        // This is not a measurement of the socket syscall alone.
        if (completed >= started) {
            const auto elapsed = completed.nanoseconds() - started.nanoseconds();
            if (elapsed > maximumSubmitIntervalNs) {
                maximumSubmitIntervalNs = elapsed;
                maximumSubmitIntervalSequence = sequence;
            }
        }
        if (started >= notBefore) {
            const auto late = started.nanoseconds() - notBefore.nanoseconds();
            if (maximumStartLatenessSequence == 0 || late > maximumStartLatenessNs) {
                maximumStartLatenessNs = late;
                maximumStartLatenessSequence = sequence;
                worstStartObservation = observation;
                worstStartObservation.submitObservedAt = started;
            }
        }
    }

    void log(const char* stage) const noexcept
    {
        if (firstSequence == 0 ||
            !mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        try {
            std::ostringstream out;
            out << "datagram_batch_trace stage=" << stage << " generation=" << generation
                << " first_sequence=" << firstSequence << " last_sequence=" << lastSequence
                << " maximum_submit_interval_ns=" << maximumSubmitIntervalNs
                << " maximum_submit_interval_sequence=" << maximumSubmitIntervalSequence
                << " maximum_start_lateness_ns=" << maximumStartLatenessNs
                << " maximum_start_lateness_sequence=" << maximumStartLatenessSequence;
            appendMediaDiagnosticMasterTime(out, "first_release", firstRelease);
            appendMediaDiagnosticMasterTime(out, "first_deadline", firstDeadline);
            appendMediaDiagnosticMasterTime(out, "last_release", lastRelease);
            appendMediaDiagnosticMasterTime(out, "last_deadline", lastDeadline);
            appendMediaDiagnosticMasterTime(out, "first_submitted", firstSubmitted);
            appendMediaDiagnosticMasterTime(out, "last_submitted", lastSubmitted);
            appendMediaDiagnosticMasterTime(out, "worst_wait_started", worstStartObservation.waitStartedAt);
            appendMediaDiagnosticMasterTime(out, "worst_wait_completed", worstStartObservation.waitCompletedAt);
            appendMediaDiagnosticMasterTime(out, "worst_submit_observed", worstStartObservation.submitObservedAt);
            waits.append(out);
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {
            // Preserve the production result if optional formatting fails.
        }
    }
};

} // namespace media::ffmpeg::graph
