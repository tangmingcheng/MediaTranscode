#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaTsMuxRuntimePlanBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaTsMuxPlan plan,
        MediaPlaybackEpoch epoch,
        MediaAvSyncGroupKey group);

    MediaBufferType type() const noexcept override;
    const MediaTsMuxPlan& plan() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAvSyncGroupKey& group() const noexcept;

private:
    MediaTsMuxRuntimePlanBuffer(MediaTsMuxPlan plan,
                                MediaPlaybackEpoch epoch,
                                MediaAvSyncGroupKey group);

    const MediaTsMuxPlan m_plan;
    const MediaPlaybackEpoch m_epoch;
    const MediaAvSyncGroupKey m_group;
};

} // namespace media::ffmpeg::graph
