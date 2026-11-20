#pragma once

#include "internal/graph/diagnostics/MediaDatagramReservationObservation.h"
#include <limits>

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
    std::uint64_t accountedSubmissions = 0;
    std::uint64_t lastAccountedSequence = 0;
    std::int64_t totalServiceNs = 0;
    std::int64_t totalStartLatenessNs = 0;
    std::int64_t totalSubmitIntervalNs = 0;
    std::int64_t totalServiceGapNs = 0;
    std::optional<MediaRunningTime> firstNotBefore;
    std::optional<MediaRunningTime> nextService;
    bool accountingIncomplete = false;

    void account(std::uint64_t sequence, MediaRunningTime notBefore,
        MediaRunningTime started, MediaRunningTime completed,
        MediaRunningTime duration) noexcept
    {
        const auto limit = (std::numeric_limits<std::int64_t>::max)();
        if (accountingIncomplete) return;
        if (notBefore.nanoseconds() < 0 || started < notBefore || completed < started ||
            duration.nanoseconds() <= 0 || completed.nanoseconds() > limit - duration.nanoseconds() ||
            (nextService && notBefore < *nextService) ||
            (accountedSubmissions != 0 && sequence <= lastAccountedSequence) ||
            accountedSubmissions == (std::numeric_limits<std::uint64_t>::max)()) {
            accountingIncomplete = true;
            return;
        }
        const auto late = started.nanoseconds() - notBefore.nanoseconds();
        const auto submit = completed.nanoseconds() - started.nanoseconds();
        const auto gap = nextService ? notBefore.nanoseconds() - nextService->nanoseconds() : 0;
        if (totalServiceNs > limit - duration.nanoseconds() || totalStartLatenessNs > limit - late ||
            totalSubmitIntervalNs > limit - submit || totalServiceGapNs > limit - gap) {
            accountingIncomplete = true;
            return;
        }
        if (!firstNotBefore) firstNotBefore = notBefore;
        totalServiceNs += duration.nanoseconds();
        totalStartLatenessNs += late;
        totalSubmitIntervalNs += submit;
        totalServiceGapNs += gap;
        nextService = MediaRunningTime::fromNanoseconds(completed.nanoseconds() + duration.nanoseconds());
        lastAccountedSequence = sequence;
        ++accountedSubmissions;
    }

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
            out << " accounted_submissions=" << accountedSubmissions
                << " last_accounted_sequence=" << lastAccountedSequence
                << " total_service_ns=" << totalServiceNs
                << " total_start_lateness_ns=" << totalStartLatenessNs
                << " total_submit_interval_ns=" << totalSubmitIntervalNs
                << " total_service_gap_ns=" << totalServiceGapNs
                << " accounting_incomplete=" << accountingIncomplete;
            appendMediaDiagnosticMasterTime(out, "first_not_before", firstNotBefore);
            appendMediaDiagnosticMasterTime(out, "next_service", nextService);
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {
            // Preserve the production result if optional formatting fails.
        }
    }
};

} // namespace media::ffmpeg::graph
