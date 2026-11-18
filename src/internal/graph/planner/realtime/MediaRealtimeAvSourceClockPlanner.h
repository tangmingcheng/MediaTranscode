#pragma once

#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSourceTimingFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyView.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanView.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpInputNodePlan;
struct MediaAudioPipelinePlan;
struct MediaAvSourceCapacityFacts;

struct MediaRealtimeAvSourceClockFixedRequest {
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    const MediaRealtimeRtpInputNodePlan& input;
    int videoStreamIndex;
    std::string_view videoCodecName;
    const MediaAudioPipelinePlan& audio;
    const MediaRealtimeRtpInputNodePlan* isolatedAudioInput;
};
struct MediaRealtimeAvSourceClockRequest final : MediaRealtimeAvSourceClockFixedRequest {
    const MediaAvSourceCapacityFacts* sourceCapacity;
};
struct MediaRealtimeAvSourceClockFixedPlan final {
    MediaRealtimeAvSourceTimingFacts timing;
    MediaRealtimeAvSyncAssemblyFixedPlan assembly;
};

struct MediaRealtimeAvSourceClockPlan final {
    MediaRealtimeAvSourceTimingFacts timing;
    MediaRealtimeAvSyncAssemblyPlan assembly;
};

class MediaRealtimeAvSourceClockPlanner final {
public:
    // Borrows fixed identities; computes real acquiring quantities before construction.
    static ::media::Result<MediaRealtimeAvSyncAssemblyView> planCapacityView(
        const MediaRealtimeAvSourceClockFixedPlan& fixed,
        const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncPlanView& synchronization);
    static ::media::Result<MediaRealtimeAvSyncAssemblyView> planCapacityView(
        MediaRealtimeAvSourceClockFixedPlan&&, const MediaRealtimeAvSourceClockRequest&, const MediaAvSyncPlanView&) = delete;
    static ::media::Result<MediaRealtimeAvSyncAssemblyView> planCapacityView(
        const MediaRealtimeAvSourceClockFixedPlan&&, const MediaRealtimeAvSourceClockRequest&, const MediaAvSyncPlanView&) = delete;
    static ::media::Result<MediaRealtimeAvSourceClockFixedPlan> prepareFixed(
        const MediaRealtimeAvSourceClockFixedRequest& request,
        const MediaAvSyncFixedPlan& synchronization);
    // Consumes the fixed owner after all candidate borrows have ended.
    static ::media::Result<MediaRealtimeAvSourceClockPlan> bind(
        MediaRealtimeAvSourceClockFixedPlan fixed,
        const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncPlanView& synchronization);
    // Fixed prepared input/timing facts; refresh acquiring capacity/time only.
    // Failure invalidates the candidate. Success does not allocate storage.
    static ::media::Status rebindAssembly(
        const MediaRealtimeAvSourceClockRequest& request, const MediaAvSyncPlanView& synchronization,
        const MediaRealtimeAvSourceTimingFacts& timing, MediaRealtimeAvSyncAssemblyView& assembly);
    static ::media::Result<MediaRealtimeAvSourceClockPlan> plan(
        const MediaRealtimeAvSourceClockRequest& request,
        const MediaAvSyncPlanView& synchronization);
private:
    friend class MediaRealtimeAvSourceRuntimePlanner;
    // Only validated planner paths can consume a capacity view.
    static ::media::Result<MediaRealtimeAvSourceClockPlan> consumeCapacity(
        MediaRealtimeAvSourceClockFixedPlan&& fixed, const MediaRealtimeAvSyncAssemblyView& capacity);
    MediaRealtimeAvSourceClockPlanner() = delete;
};

} // namespace media::ffmpeg::graph
