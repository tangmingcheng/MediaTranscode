#pragma once
#include "application/realtime/MediaRealtimeCompositionInputPreparer.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"

namespace media::ffmpeg::graph {
class MediaRealtimeCompositionPreparationState final {
public:
    // Checks all input owners; source termination is not a capability rejection.
    static ::media::Status validateInputs(
        std::span<const MediaRealtimeCompositionCapturedSource> sources,
        const MediaPreparationControl& control);
};
}
