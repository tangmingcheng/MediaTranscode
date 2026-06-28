#pragma once

#include <string>

namespace media::ffmpeg::graph {

enum class MediaGraphDiagnosticPhase {
    PlannerInput,
    PlannerCapability,
    PlannerScore,
    PlannerSelect,
    GraphBuild
};

const char* mediaGraphDiagnosticPhaseName(MediaGraphDiagnosticPhase phase) noexcept;

void mediaGraphDiagnosticLog(bool enabled,
                             MediaGraphDiagnosticPhase phase,
                             const std::string& message);

} // namespace media::ffmpeg::graph
