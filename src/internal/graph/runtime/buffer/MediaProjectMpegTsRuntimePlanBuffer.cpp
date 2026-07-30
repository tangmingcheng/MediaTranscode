#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"

#include <memory>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef>
MediaProjectMpegTsRuntimePlanBuffer::create(
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan,
    MediaPlaybackEpoch epoch,
    MediaAvSyncGroupKey group,
    std::optional<std::uint64_t> completedTransitionSequence)
{
    if (!outputPlan || !group.valid()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a valid A/V sync group"));
    }
    if (epoch.generation == 0 ||
        (completedTransitionSequence &&
         *completedTransitionSequence == 0)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a positive epoch generation"));
    }
    return ::media::Result<MediaBufferRef>::success(
        std::shared_ptr<MediaProjectMpegTsRuntimePlanBuffer>(
            new MediaProjectMpegTsRuntimePlanBuffer(
                std::move(outputPlan), epoch, std::move(group),
                completedTransitionSequence)));
}

MediaProjectMpegTsRuntimePlanBuffer::MediaProjectMpegTsRuntimePlanBuffer(
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan,
    MediaPlaybackEpoch epoch,
    MediaAvSyncGroupKey group,
    std::optional<std::uint64_t> completedTransitionSequence)
    : m_outputPlan(std::move(outputPlan))
    , m_epoch(epoch)
    , m_group(std::move(group))
    , m_completedTransitionSequence(completedTransitionSequence)
{
    setStreamKind(MediaStreamKind::Metadata);
    setPayloadKind(MediaPayloadKind::ProjectMpegTsRuntimePlan);
}

MediaBufferType MediaProjectMpegTsRuntimePlanBuffer::type() const noexcept
{
    return MediaBufferType::ProjectMpegTsRuntimePlan;
}

const MediaTsMuxPlan&
MediaProjectMpegTsRuntimePlanBuffer::muxPlan() const noexcept
{
    return m_outputPlan->protocol.muxPlan();
}

const MediaProjectMpegTsRuntimeOutputPlan&
MediaProjectMpegTsRuntimePlanBuffer::outputPlan() const noexcept
{
    return *m_outputPlan;
}

const std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan>&
MediaProjectMpegTsRuntimePlanBuffer::sharedOutputPlan() const noexcept
{
    return m_outputPlan;
}

const MediaPlaybackEpoch&
MediaProjectMpegTsRuntimePlanBuffer::epoch() const noexcept
{
    return m_epoch;
}

const MediaAvSyncGroupKey&
MediaProjectMpegTsRuntimePlanBuffer::group() const noexcept
{
    return m_group;
}

std::optional<std::uint64_t>
MediaProjectMpegTsRuntimePlanBuffer::completedTransitionSequence() const noexcept
{
    return m_completedTransitionSequence;
}

} // namespace media::ffmpeg::graph
