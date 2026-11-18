#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeGraphResourceLedgerPlan;
struct MediaAvSourceCapacityFacts;

class MediaAvSyncStartupPolicyPlanner final {
public:
    // Fixed waiting policy only; no placeholder quantity or byte capacity.
    static ::media::Result<MediaAvSyncStartupFixedPolicy> prepare(
        MediaTranscodeStreamSet members, MediaVideoStartupReadiness readiness,
        std::optional<MediaRunningTime> outputLead);
    static ::media::Result<MediaAvSyncStartupPolicy> plan(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeGraphResourceLedgerPlan& ledger,
        const MediaRealtimeDeploymentEnvelope& deployment);
    static ::media::Result<MediaAvSyncStartupPolicy> finalizePrepared(
        MediaAvSyncStartupPolicy prepared,
        const MediaRealtimeGraphResourceLedgerPlan& ledger,
        const MediaRealtimeDeploymentEnvelope& deployment);
    static ::media::Result<MediaAvSyncStartupPolicy> plan(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaAvSourceCapacityFacts& capacity,
        const MediaRealtimeDeploymentEnvelope& deployment);
    static ::media::Result<MediaAvSyncStartupPolicy> planInputPreflight(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaAvSyncStartupPolicyPlanner() = delete;
};

} // namespace media::ffmpeg::graph
