#pragma once
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceFactsPlanner.h"
#include "internal/graph/runtime/resource/MediaPreparationMemoryResource.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"
#include "internal/graph/runtime/storage/MediaFixedCapacityStorage.h"

namespace media::ffmpeg::graph {
class MediaCompositionSourceAudioPlanner final {
public:
    // Run outside freeze. Source facts and the resolved output remain unchanged
    // until their corresponding numeric result is bound into the source product.
    static ::media::Result<MediaFixedCapacityStorage<std::optional<MediaSelectedAudioResampler>>> prepare(
        std::span<const MediaRealtimeCompositionSourceFacts> sources, std::size_t selected,
        const MediaAudioPipelinePlan& selectedAudio, MediaPreparationMemoryResource& resource,
        const MediaPreparationControl& control);
};
} // namespace media::ffmpeg::graph
