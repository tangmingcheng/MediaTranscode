#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "media_transcode/Result.h"

#include <memory>
#include <optional>

namespace media::ffmpeg::graph {

class MediaProjectMpegTsRuntimePlanBuffer final : public MediaBuffer {
public:
    static ::media::Result<MediaBufferRef> create(
        std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>
            outputPlan,
        MediaPlaybackEpoch epoch,
        MediaAvSyncGroupKey group,
        std::optional<std::uint64_t> completedTransitionSequence);

    MediaBufferType type() const noexcept override;
    const MediaTsMuxPlan& muxPlan() const noexcept;
    const MediaProjectMpegTsRuntimeOutputPlan& outputPlan() const noexcept;
    const MediaPlaybackEpoch& epoch() const noexcept;
    const MediaAvSyncGroupKey& group() const noexcept;
    std::optional<std::uint64_t> completedTransitionSequence() const noexcept;

private:
    MediaProjectMpegTsRuntimePlanBuffer(
        std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>
            outputPlan,
        MediaPlaybackEpoch epoch,
        MediaAvSyncGroupKey group,
        std::optional<std::uint64_t> completedTransitionSequence);

    const std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>
        m_outputPlan;
    const MediaPlaybackEpoch m_epoch;
    const MediaAvSyncGroupKey m_group;
    const std::optional<std::uint64_t> m_completedTransitionSequence;
};

} // namespace media::ffmpeg::graph
