#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "media_transcode/Result.h"

#include <variant>

namespace media::ffmpeg::graph {

struct MediaRealtimeGraphResourceLedgerPlan;

struct MediaAvSourceContributionDomain final {};
using MediaAvSynchronizationDomain =
    std::variant<MediaAvSourceContributionDomain, MediaAvSyncPlan>;

class MediaAvSyncPlanner final {
public:
    static ::media::Result<MediaAvSyncRtpInputPlan> planRtpInputClock(
        const MediaRealtimeRtpTranscodeRequest& request,
        MediaAvSourceLifecycleMode lifecycleMode);
    static ::media::Result<MediaAvSyncPlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaTsAudioVideoSelectedProgramPlan* selectedTsProgram,
        MediaAvSynchronizationDomain domain,
        const MediaAvSyncPreparedDemuxTimestampFacts* preparedDemuxFacts,
        const MediaRealtimeGraphResourceLedgerPlan& resourceLedger,
        const MediaRealtimeDeploymentEnvelope& deployment,
        MediaBranchMode audioBranchMode,
        int resolvedOutputAudioSampleRate);

private:
    MediaAvSyncPlanner() = delete;
};

} // namespace media::ffmpeg::graph
