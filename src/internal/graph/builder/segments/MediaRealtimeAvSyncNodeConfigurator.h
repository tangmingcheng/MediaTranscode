#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaRealtimeAvSyncNodeConfigurator final {
public:
    static ::media::Result<void> configureRtpPacketClockBinder(
        MediaGraph& graph,
        MediaNodeId node,
        MediaStreamKind stream,
        const MediaRealtimeAvSyncRuntimePlan& plan);

    static ::media::Result<void> configureInitialLockedPacketGate(
        MediaGraph& graph,
        MediaNodeId node,
        MediaStreamKind stream,
        const MediaRealtimeAvSyncRuntimePlan& plan);

    static ::media::Result<void> configureCanonicalInput(
        MediaGraph& graph,
        MediaNodeId node,
        MediaScheduledStream stream,
        const MediaRealtimeAvSyncRuntimePlan& plan);

    static ::media::Result<void> configureStartupCoordinator(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSyncRuntimePlan& plan);

    static ::media::Result<void> configureStartupClock(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSyncRuntimePlan& plan);

    static ::media::Result<void> configurePlaybackEpochBinder(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSyncRuntimePlan& plan);

    static ::media::Result<void> configureActivationSequencer(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSyncRuntimePlan& plan);

private:
    MediaRealtimeAvSyncNodeConfigurator() = delete;
};

} // namespace media::ffmpeg::graph
