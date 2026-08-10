#include "internal/graph/runtime/buffer/MediaProjectMpegTsRuntimePlanBuffer.h"

#include <memory>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaBufferRef>
MediaProjectMpegTsRuntimePlanBuffer::create(
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan,
    MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaProtocolOutputActivation activation)
{
    if (!outputPlan || !sessionKey.valid()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a valid output session"));
    }
    if (activation.generation == 0 ||
        (activation.completedTransitionSequence &&
         *activation.completedTransitionSequence == 0)) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS runtime plan requires a positive epoch generation"));
    }
    return ::media::Result<MediaBufferRef>::success(
        std::shared_ptr<MediaProjectMpegTsRuntimePlanBuffer>(
            new MediaProjectMpegTsRuntimePlanBuffer(
                std::move(outputPlan), std::move(sessionKey), streamSet,
                activation)));
}

MediaProjectMpegTsRuntimePlanBuffer::MediaProjectMpegTsRuntimePlanBuffer(
    std::shared_ptr<const MediaProjectMpegTsRuntimeOutputPlan> outputPlan,
    MediaProtocolOutputSessionKey sessionKey,
    MediaTranscodeStreamSet streamSet,
    MediaProtocolOutputActivation activation)
    : m_outputPlan(std::move(outputPlan))
    , m_sessionKey(std::move(sessionKey))
    , m_streamSet(streamSet)
    , m_activation(activation)
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

const MediaProtocolOutputSessionKey&
MediaProjectMpegTsRuntimePlanBuffer::sessionKey() const noexcept
{
    return m_sessionKey;
}

MediaTranscodeStreamSet
MediaProjectMpegTsRuntimePlanBuffer::streamSet() const noexcept
{
    return m_streamSet;
}

const MediaProtocolOutputActivation&
MediaProjectMpegTsRuntimePlanBuffer::activation() const noexcept
{
    return m_activation;
}

} // namespace media::ffmpeg::graph
