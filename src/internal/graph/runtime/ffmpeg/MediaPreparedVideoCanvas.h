#pragma once

#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasProducer.h"
#include "internal/graph/planner/realtime/MediaVideoCanvasPreparationPlanner.h"
#include <mutex>

namespace media::ffmpeg::graph {

// Prepared after logical topology admission and before runtime binding; owns the exact production surfaces until
// the aggregate owner claims them. No node wakeup exists during preparation.
class MediaPreparedVideoCanvas final {
public:
    static ::media::Result<std::shared_ptr<MediaPreparedVideoCanvas>> prepare(
        const MediaVideoCanvasPreparationPlan& plan, AVBufferRef* productionHwFrames,
        std::span<const AVFrame* const> preparedTiles,
        const std::shared_ptr<MediaPreparationStorageBudget>& payloadBudget,
        const MediaPreparationControl& control);
    ::media::Status validateBinding(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames) const;
    ::media::Result<std::unique_ptr<MediaVideoCanvasProducer>> claim(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
        std::shared_ptr<MediaNodeWakeup> availabilityWakeup);
private:
    MediaPreparedVideoCanvas() = default;
    ::media::Status validateBindingLocked(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames) const;
    // Declared first: release after the producer and its frames. Published
    // frames share the same lease even when this prepared owner is gone.
    std::shared_ptr<const MediaPreparationStorageLease> payloadStorage_;
    mutable std::mutex mutex_;
    std::unique_ptr<MediaVideoCanvasProducer> producer_;
};

} // namespace media::ffmpeg::graph
