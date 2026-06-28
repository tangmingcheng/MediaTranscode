#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include <spdlog/spdlog.h>

namespace media::ffmpeg::graph {

const char* mediaGraphDiagnosticPhaseName(MediaGraphDiagnosticPhase phase) noexcept
{
    switch (phase) {
    case MediaGraphDiagnosticPhase::PlannerInput:
        return "planner.input";
    case MediaGraphDiagnosticPhase::PlannerCapability:
        return "planner.capability";
    case MediaGraphDiagnosticPhase::PlannerScore:
        return "planner.score";
    case MediaGraphDiagnosticPhase::PlannerSelect:
        return "planner.select";
    case MediaGraphDiagnosticPhase::GraphBuild:
        return "builder.graph";
    }
    return "unknown";
}

void mediaGraphDiagnosticLog(bool enabled,
                             MediaGraphDiagnosticPhase phase,
                             const std::string& message)
{
    if (!enabled) {
        return;
    }

    spdlog::info("[graph][{}] {}", mediaGraphDiagnosticPhaseName(phase), message);
}

} // namespace media::ffmpeg::graph
