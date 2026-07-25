#include "internal/graph/runtime/buffer/MediaTsMuxRuntimePlanBuffer.h"

#include <memory>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef> MediaTsMuxRuntimePlanBuffer::create(
    MediaTsMuxPlan plan,
    MediaPlaybackEpoch epoch,
    MediaAvSyncGroupKey group)
{
    if (!group.valid()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a valid A/V sync group"));
    }
    if (epoch.generation == 0) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a positive epoch generation"));
    }
    return ::media::Result<MediaBufferRef>::success(
        std::shared_ptr<MediaTsMuxRuntimePlanBuffer>(
            new MediaTsMuxRuntimePlanBuffer(
                std::move(plan), epoch, std::move(group))));
}

MediaTsMuxRuntimePlanBuffer::MediaTsMuxRuntimePlanBuffer(
    MediaTsMuxPlan plan,
    MediaPlaybackEpoch epoch,
    MediaAvSyncGroupKey group)
    : m_plan(std::move(plan))
    , m_epoch(epoch)
    , m_group(std::move(group))
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::TsMuxRuntimePlan);
}

MediaBufferType MediaTsMuxRuntimePlanBuffer::type() const noexcept
{
    return MediaBufferType::TsMuxRuntimePlan;
}

const MediaTsMuxPlan& MediaTsMuxRuntimePlanBuffer::plan() const noexcept
{
    return m_plan;
}

const MediaPlaybackEpoch& MediaTsMuxRuntimePlanBuffer::epoch() const noexcept
{
    return m_epoch;
}

const MediaAvSyncGroupKey& MediaTsMuxRuntimePlanBuffer::group() const noexcept
{
    return m_group;
}

} // namespace media::ffmpeg::graph
