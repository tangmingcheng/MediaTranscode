#pragma once

#include "internal/graph/diagnostics/MediaAccessUnitProcessingTrace.h"
#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

// Fixed current-AU aggregates. Times are steady-clock observations, not socket
// timestamps. Missing evidence never changes a media transaction's result.
struct MediaTsPrefixCommitObservation final {
    std::uint64_t published = 0;
    std::uint64_t completed = 0;
    std::uint64_t samples = 0;
    std::uint64_t wouldBlock = 0;
    std::uint64_t publishToCommitTotal = 0;
    std::uint64_t publishToCommitMaximum = 0;
    std::uint64_t publishToCommitMaximumOffset = 0;
    std::uint64_t commitToResumeTotal = 0;
    std::uint64_t commitToResumeMaximum = 0;
    std::uint64_t commitToResumeMaximumOffset = 0;
    bool incomplete = false;
    std::optional<std::int64_t> publishedAt;

    void increment(std::uint64_t& count) noexcept
    {
        if (count == (std::numeric_limits<std::uint64_t>::max)()) incomplete = true;
        else ++count;
    }
    void recordPublished() noexcept
    {
        if (publishedAt) incomplete = true;
        publishedAt = mediaAccessUnitObservationNow();
        if (!publishedAt) incomplete = true;
        increment(published);
    }
    void recordCompleted(std::optional<std::int64_t> committedAt) noexcept
    {
        const auto resumedAt = mediaAccessUnitObservationNow();
        const auto start = std::exchange(publishedAt, std::nullopt);
        const auto offset = completed;
        increment(completed);
        if (!start || !committedAt || !resumedAt || *start < 0 ||
            *committedAt < *start || *resumedAt < *committedAt) {
            incomplete = true;
            return;
        }
        const auto first = static_cast<std::uint64_t>(*committedAt - *start);
        const auto second = static_cast<std::uint64_t>(*resumedAt - *committedAt);
        const auto limit = (std::numeric_limits<std::uint64_t>::max)();
        if (first > limit - publishToCommitTotal || second > limit - commitToResumeTotal) {
            incomplete = true;
            return;
        }
        publishToCommitTotal += first;
        commitToResumeTotal += second;
        if (first > publishToCommitMaximum) {
            publishToCommitMaximum = first;
            publishToCommitMaximumOffset = offset;
        }
        if (second > commitToResumeMaximum) {
            commitToResumeMaximum = second;
            commitToResumeMaximumOffset = offset;
        }
        increment(samples);
    }
    void log(const char* stage, const MediaAccessUnitMuxObservation& au) const noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        try {
            std::ostringstream out;
            out << "ts_prefix_commit_summary stage=" << stage
                << " generation=" << au.generation << " stream=" << au.stream
                << " canonical_sequence=" << au.processing.canonicalSequence
                << " emit_master_ns=" << au.emitMasterNs << " dispatch_master_ns=" << au.dispatchMasterNs
                << " published=" << published << " completed=" << completed << " samples=" << samples
                << " would_block=" << wouldBlock << " incomplete=" << incomplete
                << " publish_to_protocol_commit_completed_total_ns=" << publishToCommitTotal
                << " publish_to_protocol_commit_completed_max_ns=" << publishToCommitMaximum
                << " publish_to_protocol_commit_completed_max_prefix_offset=" << publishToCommitMaximumOffset
                << " protocol_commit_completed_to_mux_resume_total_ns=" << commitToResumeTotal
                << " protocol_commit_completed_to_mux_resume_max_ns=" << commitToResumeMaximum
                << " protocol_commit_completed_to_mux_resume_max_prefix_offset=" << commitToResumeMaximumOffset;
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {}
    }
};

} // namespace media::ffmpeg::graph
