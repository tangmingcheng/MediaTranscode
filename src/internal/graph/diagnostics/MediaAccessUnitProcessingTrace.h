#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/time/MediaSteadyClock.h"
#include "internal/graph/diagnostics/MediaCodecReceiveObservation.h"

#include <optional>
#include <sstream>

namespace media::ffmpeg::graph {

inline std::optional<std::int64_t> mediaAccessUnitObservationNow() noexcept
{
    return mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)
        ? std::optional<std::int64_t>(mediaSteadyClockNowNs()) : std::nullopt;
}

// Immutable after publication. These are construction/dequeue observations,
// not successful-publication timestamps or a scheduling contract.
struct MediaAccessUnitProcessingTrace final {
    std::uint64_t canonicalSequence;
    std::optional<std::int64_t> scheduledCreated;
    std::optional<std::int64_t> adapterDequeued;
    std::optional<std::int64_t> adaptedCreated;
    std::optional<MediaCodecReceiveObservation> codecReceive;

    void logMux(const char* stage, std::uint64_t generation, const char* stream,
                    std::int64_t emitMasterNs, std::int64_t dispatchMasterNs, std::int64_t readyMasterNs,
                    std::optional<std::int64_t> writeEntered,
                    std::optional<std::int64_t> sessionReady) const noexcept
    {
        if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return;
        try {
            std::ostringstream message;
            message << "au_processing_trace stage=" << stage << " generation=" << generation
                    << " stream=" << stream << " canonical_sequence=" << canonicalSequence
                    << " emit_master_ns=" << emitMasterNs
                    << " dispatch_master_ns=" << dispatchMasterNs
                    << " ready_master_ns=" << readyMasterNs;
            const auto append = [&](const char* name, std::optional<std::int64_t> time) {
                message << ' ' << name << "_steady_ns=";
                if (time) message << *time;
                else message << "unavailable";
            };
            append("scheduled_created", scheduledCreated);
            append("adapter_dequeued", adapterDequeued);
            append("adapted_created", adaptedCreated);
            append("write_entered", writeEntered);
            append("session_ready", sessionReady);
            if (codecReceive) {
                append("codec_receive_begin", codecReceive->beginSteadyNs);
                append("codec_receive_end", codecReceive->endSteadyNs);
            }
            if (codecReceive && codecReceive->token) codecReceive->token->append(message);
            else message << " codec_identity=unavailable";
            mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                MediaGraphDiagnosticPhase::RuntimeNode, message.str());
        } catch (...) {
            // Preserve the original media result even if logging cannot allocate.
        }
    }
};

// One current AU only; owned and accessed under the mux session's existing
// serialized generation permit. No shared mutable publication timestamp.
struct MediaAccessUnitMuxObservation final {
    MediaAccessUnitProcessingTrace processing;
    std::uint64_t generation;
    const char* stream;
    std::int64_t emitMasterNs;
    std::int64_t dispatchMasterNs;
    std::int64_t readyMasterNs;
    std::optional<std::int64_t> writeEntered;
    std::optional<std::int64_t> sessionReady;

    void log(const char* stage) const noexcept
    {
        processing.logMux(stage, generation, stream, emitMasterNs,
            dispatchMasterNs, readyMasterNs, writeEntered, sessionReady);
    }
};

} // namespace media::ffmpeg::graph
