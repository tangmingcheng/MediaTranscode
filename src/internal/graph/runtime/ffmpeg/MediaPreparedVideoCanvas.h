#pragma once

#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasProducer.h"
#include <mutex>

namespace media::ffmpeg::graph {

// Prepared before DAG construction; owns the exact production surfaces until
// the aggregate owner claims them. No node wakeup exists during preparation.
class MediaPreparedVideoCanvas final {
public:
    static ::media::Result<std::shared_ptr<MediaPreparedVideoCanvas>> prepare(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
        std::span<const AVFrame* const> preparedTiles);
    ::media::Status validateBinding(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames) const;
    ::media::Result<std::unique_ptr<MediaVideoCanvasProducer>> claim(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
        std::shared_ptr<MediaNodeWakeup> availabilityWakeup);
private:
    MediaPreparedVideoCanvas() = default;
    ::media::Status validateBindingLocked(
        const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames) const;
    mutable std::mutex mutex_;
    std::unique_ptr<MediaVideoCanvasProducer> producer_;
};

} // namespace media::ffmpeg::graph
