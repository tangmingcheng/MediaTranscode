#pragma once
#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/planner/realtime/MediaEncoderHardwareFramesPoolPlanner.h"
namespace media::ffmpeg::graph {
class MediaVideoOutputPreparationPlanner;
class MediaVideoOutputPreparationPlan final {
public:
    const MediaEncoderHardwareFramesPoolPlan& pool() const noexcept { return pool_; }
    const MediaVideoCanvasAllocation& allocation() const noexcept { return allocation_; }
    std::size_t storageBytes() const noexcept { return storageBytes_; }
private:
    friend class MediaVideoOutputPreparationPlanner;
    MediaVideoOutputPreparationPlan(MediaEncoderHardwareFramesPoolPlan pool,
        MediaVideoCanvasAllocation allocation, std::size_t storageBytes)
        : pool_(std::move(pool)), allocation_(allocation), storageBytes_(storageBytes) {}
    MediaEncoderHardwareFramesPoolPlan pool_;
    MediaVideoCanvasAllocation allocation_;
    std::size_t storageBytes_;
};
} // namespace media::ffmpeg::graph
