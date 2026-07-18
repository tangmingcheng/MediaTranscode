#pragma once

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>

namespace media_transcode::test {

class ScheduledRtpDecodeSampleFixture;

class ScheduledRtpOutputIntegrationRuntime final {
public:
    static ::media::Result<ScheduledRtpOutputIntegrationRuntime>
    openSendersAndPublish(
        const ::media::ffmpeg::graph::MediaRealtimeAvSyncRuntimePlan& plan,
        AVCodecContext& videoCodec,
        AVCodecContext& audioCodec);

    ScheduledRtpOutputIntegrationRuntime(
        ScheduledRtpOutputIntegrationRuntime&&) noexcept;
    ScheduledRtpOutputIntegrationRuntime& operator=(
        ScheduledRtpOutputIntegrationRuntime&&) noexcept;
    ScheduledRtpOutputIntegrationRuntime(
        const ScheduledRtpOutputIntegrationRuntime&) = delete;
    ScheduledRtpOutputIntegrationRuntime& operator=(
        const ScheduledRtpOutputIntegrationRuntime&) = delete;
    ~ScheduledRtpOutputIntegrationRuntime();

    ::media::Status sendAccessUnits(
        const ScheduledRtpDecodeSampleFixture& sample);

private:
    ScheduledRtpOutputIntegrationRuntime(
        std::unique_ptr<::media::ffmpeg::graph::MediaGraphRuntime> runtime,
        std::shared_ptr<::media::ffmpeg::graph::MediaMasterClock> clock,
        ::media::ffmpeg::graph::MediaNodeId scheduler,
        ::media::ffmpeg::graph::MediaNodeId router,
        ::media::ffmpeg::graph::MediaNodeId videoSender,
        ::media::ffmpeg::graph::MediaNodeId audioSender,
        ::media::ffmpeg::graph::MediaRunningTime videoLead,
        ::media::ffmpeg::graph::MediaRunningTime audioLead) noexcept;

    std::unique_ptr<::media::ffmpeg::graph::MediaGraphRuntime> m_runtime;
    std::shared_ptr<::media::ffmpeg::graph::MediaMasterClock> m_clock;
    ::media::ffmpeg::graph::MediaNodeId m_scheduler;
    ::media::ffmpeg::graph::MediaNodeId m_router;
    ::media::ffmpeg::graph::MediaNodeId m_videoSender;
    ::media::ffmpeg::graph::MediaNodeId m_audioSender;
    ::media::ffmpeg::graph::MediaRunningTime m_videoLead;
    ::media::ffmpeg::graph::MediaRunningTime m_audioLead;
};

} // namespace media_transcode::test
