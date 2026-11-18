#pragma once

#include "internal/graph/model/MediaVideoCanvasPlan.h"
#include "internal/graph/planner/realtime/MediaVideoCanvasRetentionPlanner.h"
#include <utility>

namespace media::ffmpeg::graph {

// Only exposed image payload while owned by the canvas. Not a whole-resource
// ledger: excludes headers, metadata, pool caches and opaque driver allocations.
class MediaVideoCanvasPreparationPlan final {
public:
    const MediaVideoCanvasPlan& canvas() const noexcept { return canvas_; }
    std::size_t retainedPayloadBytes() const noexcept { return retainedPayloadBytes_; }
    std::size_t transientPayloadBytes() const noexcept { return transientPayloadBytes_; }
private:
    friend class MediaVideoCanvasPreparationPlanner;
    MediaVideoCanvasPreparationPlan(MediaVideoCanvasPlan canvas,
        std::size_t retained, std::size_t transient)
        : canvas_(std::move(canvas)), retainedPayloadBytes_(retained),
          transientPayloadBytes_(transient) {}
    MediaVideoCanvasPlan canvas_;
    std::size_t retainedPayloadBytes_;
    std::size_t transientPayloadBytes_;
};

class MediaVideoCanvasPreparationPlanner final {
public:
    static ::media::Result<MediaVideoCanvasPreparationPlan> plan(
        const MediaVideoCanvasGeometry& geometry,
        const MediaVideoCanvasRetentionPlan& retention,
        const MediaVideoCanvasAllocation& allocation);
};

} // namespace media::ffmpeg::graph
