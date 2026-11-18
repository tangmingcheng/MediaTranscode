#include "application/realtime/MediaRealtimeCompositionPreparationState.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

namespace media::ffmpeg::graph {
::media::Status MediaRealtimeCompositionPreparationState::validateInputs(
    std::span<const MediaRealtimeCompositionCapturedSource> sources,
    const MediaPreparationControl& control)
{
    std::optional<::media::ErrorInfo> failure;
    std::size_t failedSource = 0;
    for (std::size_t i = 0; i < sources.size(); ++i) {
        auto status = sources[i].capture.validateContinuousPreparation(sources[i].session);
        if (!status && (!failure || failure->code == ::media::ErrorCode::Cancelled)) {
            failure = status.error();
            failedSource = i;
        }
    }
    if (failure) {
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
            MediaGraphDiagnosticPhase::PlannerCapability,
            "composition_preparation_input_terminal source=" + std::to_string(failedSource) +
            " error=" + failure->describe());
        return ::media::Status::failure(std::move(*failure));
    }
    return control.check("composition candidate preparation");
}
}
