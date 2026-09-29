#pragma once

#include "internal/graph/runtime/ffmpeg/MediaVideoCanvasAdapter.h"
#include "internal/graph/runtime/resource/MediaPreparationStorageBudget.h"
#include "internal/graph/runtime/resource/MediaPreparationControl.h"
#include <atomic>
#include <span>

namespace media::ffmpeg::graph {

class MediaNodeWakeup;
class MediaPreparedVideoCanvas;

// Serialized by the owning node; no worker thread or deferred operation queue.
class MediaVideoCanvasProducer final {
public:
    ::media::Result<FramePtr> cloneBlack();
    // One entry per planned tile; null means black for this output deadline.
    ::media::Result<FramePtr> compose(std::span<const AVFrame* const> tiles);
private:
    friend class MediaPreparedVideoCanvas;
    ::media::Status prepare(const MediaVideoCanvasPlan& plan, AVBufferRef* productionHwFrames,
        std::shared_ptr<const MediaPreparationStorageLease> payloadStorage,
        const MediaPreparationControl& control);
    ::media::Result<FramePtr> publish(const AVFrame& frame);
    // Last released; output HeaderLease objects also retain this ownership.
    std::shared_ptr<const MediaPreparationStorageLease> payloadStorage_;
    MediaVideoCanvasPlan plan_{};
    BufferRefPtr frames_;
    std::unique_ptr<MediaVideoCanvasAdapter> adapter_;
    FramePtr black_;
    std::vector<FramePtr> surfaces_;
    std::shared_ptr<std::atomic_size_t> outstanding_;
    std::shared_ptr<MediaNodeWakeup> availabilityWakeup_;
};

} // namespace media::ffmpeg::graph
