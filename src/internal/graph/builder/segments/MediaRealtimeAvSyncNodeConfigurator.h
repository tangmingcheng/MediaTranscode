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
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configureDemuxPacketClockBinder(
        MediaGraph& graph,
        MediaNodeId node,
        MediaStreamKind stream,
        const MediaAvSyncGroupKey& syncGroup,
        const MediaDemuxTimestampInputClockAssemblyPlan& plan);

    static ::media::Result<void> configureLockedPacketGate(
        MediaGraph& graph,
        MediaNodeId node,
        MediaStreamKind stream,
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configureCanonicalInput(
        MediaGraph& graph,
        MediaNodeId node,
        MediaScheduledStream stream,
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configureStartupCoordinator(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configureStartupClock(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configurePlaybackEpochBinder(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configureActivationSequencer(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSourceRuntimePlan& plan);

    static ::media::Result<void> configureBoundReleaseExtractor(
        MediaGraph& graph,
        MediaNodeId node,
        const MediaRealtimeAvSourceRuntimePlan& plan);

private:
    MediaRealtimeAvSyncNodeConfigurator() = delete;
};

} // namespace media::ffmpeg::graph
