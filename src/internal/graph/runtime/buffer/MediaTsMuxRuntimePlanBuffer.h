#pragma once

#include "internal/graph/protocol/mpegts/MediaTsMuxPlan.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "media_transcode/Result.h"

#include <optional>

namespace media::ffmpeg::graph {

class MediaTsMuxRuntimePlanBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        MediaTsMuxPlan plan,
        MediaPlaybackEpoch epoch,
        MediaAvSyncGroupKey group,
        std::optional<std::uint64_t> completedTransitionSequence);

    MediaBufferType type() const noexcept override;
    const MediaTsMuxPlan& plan() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAvSyncGroupKey& group() const noexcept;
    std::optional<std::uint64_t> completedTransitionSequence() const noexcept;

private:
    MediaTsMuxRuntimePlanBuffer(MediaTsMuxPlan plan,
                                MediaPlaybackEpoch epoch,
                                MediaAvSyncGroupKey group,
                                std::optional<std::uint64_t>
                                    completedTransitionSequence);

    const MediaTsMuxPlan m_plan;
    const MediaPlaybackEpoch m_epoch;
    const MediaAvSyncGroupKey m_group;
    const std::optional<std::uint64_t> m_completedTransitionSequence;
};

} // namespace media::ffmpeg::graph
