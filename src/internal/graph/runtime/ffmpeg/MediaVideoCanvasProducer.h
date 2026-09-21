#pragma once

#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
#include <atomic>
#include <span>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;

// Serialized by the owning node; no worker thread or deferred operation queue.
class MediaVideoCanvasProducer final {
public:
    ::media::Status prepare(const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
                           std::shared_ptr<MediaNodeWakeup> availabilityWakeup);
    ::media::Result<FramePtr> cloneBlack();
    // One entry per planned tile; null means black for this output deadline.
    ::media::Result<FramePtr> compose(std::span<const AVFrame* const> tiles);
private:
    ::media::Result<FramePtr> publish(const AVFrame& frame);
    MediaVideoCanvasPlan plan_{};
    BufferRefPtr frames_;
    std::unique_ptr<MediaVideoCanvasAdapter> adapter_;
    FramePtr black_;
    std::vector<FramePtr> surfaces_;
    std::shared_ptr<std::atomic_size_t> outstanding_;
    std::shared_ptr<MediaNodeWakeup> availabilityWakeup_;
};

} // namespace media::ffmpeg::graph
