#pragma once

#include "internal/graph/diagnostics/MediaDiagnosticMasterClock.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"
#include "internal/graph/runtime/buffer/MediaMpegTsProtocolDatagramBatchBuffer.h"
#include "internal/graph/runtime/buffer/MediaWireGlobalSequenceState.h"

#include <algorithm>
#include <array>
#include <optional>
#include <sstream>

namespace media::ffmpeg::graph {

// Bounded observations only; none of these values participates in scheduling.
struct MediaProtocolBatchMaterializationSummary {
    std::optional<MediaRunningTime> firstDequeuedAt;
    std::optional<MediaRunningTime> lastWirePublishedAt;
    std::uint64_t attempts = 0;
    std::optional<MediaWireCompletedReservationObservation> completedReservationAtLastAttempt;
    std::optional<MediaRunningTime> previousAttemptStartedAt;
    std::optional<MediaRunningTime> previousAttemptCompletedAt;
    std::optional<MediaRunningTime> lastAttemptStartedAt;
    std::optional<MediaRunningTime> lastAttemptCompletedAt;
    std::optional<MediaRunningTime> lastBlockedStartedAt;
    std::optional<MediaRunningTime> lastBlockedCompletedAt;
    std::uint64_t blockedAttempts = 0;
    // Diagnostic text capacity only, never a media or scheduling limit.
    std::array<char, 1024> blockedMessage{};
    std::size_t blockedMessageBytes = 0;
    std::size_t blockedOriginalMessageBytes = 0;
    int blockedNativeCode = 0;

};

struct MediaProtocolBatchMaterializationObservation final
    : MediaProtocolBatchMaterializationSummary {
    std::optional<MediaProtocolBatchMaterializationSummary> previousBatch;

    void beginBatch(std::optional<MediaRunningTime> dequeuedAt) noexcept
    {
        const auto published = lastWirePublishedAt;
        static_cast<MediaProtocolBatchMaterializationSummary&>(*this) = {};
        firstDequeuedAt = dequeuedAt;
        lastWirePublishedAt = published;
    }

    // Called only after the final wire output of this protocol batch is published.
    void completeBatch() noexcept
    {
        previousBatch = static_cast<const MediaProtocolBatchMaterializationSummary&>(*this);
    }

    void recordAttempt(MediaRunningTime startedAt,
        std::optional<MediaRunningTime> completedAt,
        const ::media::ErrorInfo* error) noexcept
    {
        ++attempts;
        previousAttemptStartedAt = lastAttemptStartedAt;
        previousAttemptCompletedAt = lastAttemptCompletedAt;
        lastAttemptStartedAt = startedAt;
        lastAttemptCompletedAt = completedAt;
        if (!error || error->code != ::media::ErrorCode::WouldBlock) return;
        ++blockedAttempts;
        lastBlockedStartedAt = startedAt;
        lastBlockedCompletedAt = completedAt;
        blockedNativeCode = error->nativeCode;
        blockedOriginalMessageBytes = error->message.size();
        blockedMessageBytes = (std::min)(blockedOriginalMessageBytes, blockedMessage.size());
        std::copy_n(error->message.data(), blockedMessageBytes, blockedMessage.data());
    }

    void logFailure(const MediaMpegTsProtocolDatagramBatchBuffer& batch,
        MediaRunningTime attemptedAt, const ::media::ErrorInfo& error) const noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        try {
            std::ostringstream out;
            out << "protocol_batch_trace stage=materialization_failed generation=" << batch.generation()
                << " attempts=" << attempts << " consumed_datagrams=" << batch.materializedDatagrams()
                << " remaining_datagrams=" << batch.datagrams().size()
                << " attempted_master_ns=" << attemptedAt.nanoseconds();
            const auto append = [&](const char* name, std::optional<MediaRunningTime> value) {
                appendMediaDiagnosticMasterTime(out, name, value);
            };
            append("mux_dequeued", batch.muxDequeuedAt());
            append("first_dequeued", firstDequeuedAt);
            append("last_wire_published", lastWirePublishedAt);
            append("previous_attempt_started", previousAttemptStartedAt);
            append("previous_attempt_completed", previousAttemptCompletedAt);
            append("last_attempt_completed", lastAttemptCompletedAt);
            append("last_blocked_started", lastBlockedStartedAt);
            append("last_blocked_completed", lastBlockedCompletedAt);
            out << " blocked_attempts=" << blockedAttempts;
            if (blockedAttempts) {
                out << " last_blocked_code=WouldBlock last_blocked_native=" << blockedNativeCode
                    << " last_blocked_original_message_bytes=" << blockedOriginalMessageBytes
                    << " last_blocked_message_truncated=" << (blockedMessageBytes < blockedOriginalMessageBytes)
                    << " last_blocked_message=\"";
                out.write(blockedMessage.data(), static_cast<std::streamsize>(blockedMessageBytes));
                out << '\"';
            }
            if (previousBatch) {
                const auto& previous = *previousBatch;
                append("previous_batch_first_dequeued", previous.firstDequeuedAt);
                append("previous_batch_last_attempt_started", previous.lastAttemptStartedAt);
                append("previous_batch_last_attempt_completed", previous.lastAttemptCompletedAt);
                append("previous_batch_last_blocked_started", previous.lastBlockedStartedAt);
                append("previous_batch_last_blocked_completed", previous.lastBlockedCompletedAt);
                append("previous_batch_final_wire_published", previous.lastWirePublishedAt);
                out << " previous_batch_attempts=" << previous.attempts
                    << " previous_batch_blocked_attempts=" << previous.blockedAttempts;
                if (previous.completedReservationAtLastAttempt) {
                    const auto& completed = *previous.completedReservationAtLastAttempt;
                    out << " previous_batch_observed_completed_reservation_id=" << completed.identity
                        << " previous_batch_observed_completed_reservation_first_sequence=" << completed.firstSequence
                        << " previous_batch_observed_completed_reservation_last_sequence=" << completed.lastSequence;
                    append("previous_batch_observed_completed_reservation_submitted", completed.submittedAt);
                }
                if (previous.blockedAttempts) {
                    // reserve() captures head_id under its ledger lock. Retain that
                    // exact rejection; a later snapshot head is not the blocked owner.
                    out << " previous_batch_last_blocked_native=" << previous.blockedNativeCode
                        << " previous_batch_last_blocked_original_message_bytes=" << previous.blockedOriginalMessageBytes
                        << " previous_batch_last_blocked_message_truncated="
                        << (previous.blockedMessageBytes < previous.blockedOriginalMessageBytes)
                        << " previous_batch_last_blocked_message=\"";
                    out.write(previous.blockedMessage.data(),
                        static_cast<std::streamsize>(previous.blockedMessageBytes));
                    out << '\"';
                }
            }
            if (completedReservationAtLastAttempt) {
                const auto& completed = *completedReservationAtLastAttempt;
                out << " global_last_completed_reservation_id=" << completed.identity
                    << " global_last_completed_reservation_first_sequence=" << completed.firstSequence
                    << " global_last_completed_reservation_last_sequence=" << completed.lastSequence;
                append("global_last_completed_reservation_submitted", completed.submittedAt);
            }
            if (!batch.datagrams().empty()) {
                out << " remaining_front_release_master_ns=" << batch.datagrams().front().canonicalRelease().nanoseconds()
                    << " remaining_front_deadline_master_ns=" << batch.datagrams().front().canonicalDeadline().nanoseconds()
                    << " remaining_front_completion_limit_master_ns=" << batch.datagrams().front().completionLimit().nanoseconds();
            }
            // The front need not be the failing datagram. Preserve the exact
            // failure's release/deadline in the original error alongside it.
            out << " original_error=\"" << error.describe() << '\"';
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, out.str());
        } catch (...) {
            // Observation failure must not replace the production error.
        }
    }
};

} // namespace media::ffmpeg::graph
