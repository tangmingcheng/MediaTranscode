#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaDatagramShapingPlanBuffer final : public MediaBuffer {
public:
    static ::media::Result<std::shared_ptr<MediaDatagramShapingPlanBuffer>>
    create(MediaDatagramShapingPlan plan);

    MediaBufferType type() const noexcept override
    {
        return MediaBufferType::DatagramShapingPlan;
    }
    const MediaDatagramShapingPlan& plan() const noexcept { return m_plan; }
    ::media::Result<std::shared_ptr<MediaDatagramShapingPlanBuffer>>
    clone() const;

private:
    explicit MediaDatagramShapingPlanBuffer(
        MediaDatagramShapingPlan plan) noexcept;

    MediaDatagramShapingPlan m_plan;
};

} // namespace media::ffmpeg::graph
