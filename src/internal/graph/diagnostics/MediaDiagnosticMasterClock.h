#pragma once

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/time/MediaMasterClock.h"

#include <optional>
#include <ostream>

namespace media::ffmpeg::graph {

inline std::optional<MediaRunningTime> mediaDiagnosticMasterClockNow(
    const MediaMasterClock& clock) noexcept
{
    if (!mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel::State)) return std::nullopt;
    auto time = clock.now();
    return time ? std::optional(time.value()) : std::nullopt;
}

inline void appendMediaDiagnosticMasterTime(std::ostream& out,
    const char* name, std::optional<MediaRunningTime> value)
{
    out << ' ' << name << "_master_ns=";
    if (value) out << value->nanoseconds();
    else out << "unavailable";
}

} // namespace media::ffmpeg::graph
