#pragma once

#include "internal/graph/nodes/mux/ScheduledRtpSenderConfig.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/rtp/MediaRtpUdpSenderConfig.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"
#include "internal/graph/time/MediaSharedNtpEpoch.h"

struct AVCodecContext;

namespace media::ffmpeg::graph {

class MediaScheduledRtpSenderMaterialization final {
public:
    MediaScheduledRtpSenderMaterialization(
        MediaScheduledRtpSenderMaterialization&&) noexcept = default;
    MediaScheduledRtpSenderMaterialization& operator=(
        MediaScheduledRtpSenderMaterialization&&) noexcept = default;
    MediaScheduledRtpSenderMaterialization(
        const MediaScheduledRtpSenderMaterialization&) = delete;
    MediaScheduledRtpSenderMaterialization& operator=(
        const MediaScheduledRtpSenderMaterialization&) = delete;

    MediaRtpUdpSenderConfig releaseTransportConfig() noexcept;
    ScheduledRtpSenderConfig releaseSenderConfig() noexcept;
    MediaBufferRef releaseDescription() noexcept;

private:
    friend class MediaScheduledRtpSenderMaterializer;

    MediaScheduledRtpSenderMaterialization(
        MediaRtpUdpSenderConfig transportConfig,
        ScheduledRtpSenderConfig senderConfig,
        MediaBufferRef description) noexcept;

    MediaRtpUdpSenderConfig m_transportConfig;
    ScheduledRtpSenderConfig m_senderConfig;
    MediaBufferRef m_description;
};

class MediaScheduledRtpSenderMaterializer final {
public:
    static ::media::Result<MediaScheduledRtpSenderMaterialization> materialize(
        const MediaScheduledRtpOutputPlan& outputPlan,
        const MediaSeparateRtpSdpRuntimePlan& sdpPlan,
        const AVCodecContext& codecContext,
        const MediaSharedNtpEpoch& sharedNtpEpoch,
        const MediaPlaybackEpoch& playbackEpoch);
};

} // namespace media::ffmpeg::graph
