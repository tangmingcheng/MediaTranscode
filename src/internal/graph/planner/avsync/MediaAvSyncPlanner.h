#pragma once

#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaTsProgramSelector.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "media_transcode/Result.h"

#include <variant>
#include <functional>

namespace media::ffmpeg::graph {

struct MediaRealtimeGraphResourceLedgerPlan;
struct MediaAvSourceCapacityFacts;
using MediaAvSyncCapacityFacts = std::variant<
    std::reference_wrapper<const MediaRealtimeGraphResourceLedgerPlan>,
    std::reference_wrapper<const MediaAvSourceCapacityFacts>>;

struct MediaAvSourceContributionDomain final {};
using MediaAvSynchronizationDomain =
    std::variant<MediaAvSourceContributionDomain, MediaAvSyncPlan>;

class MediaAvSyncPlanner final {
public:
    // Incomplete owning facts: cannot be passed to a runtime-plan validator.
    static ::media::Result<MediaAvSyncFixedPlan> prepareFixed(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaTsAudioVideoSelectedProgramPlan* selectedTsProgram,
        MediaAvSynchronizationDomain domain,
        const MediaAvSyncPreparedDemuxTimestampFacts* preparedDemuxFacts,
        MediaAvSyncStartupFixedPolicy startup,
        MediaBranchMode audioBranchMode,
        std::optional<int> resolvedOutputAudioSampleRate);
    // Consume the fixed owner only after candidate borrows have ended.
    static ::media::Result<MediaAvSyncPlan> bind(
        MediaAvSyncFixedPlan fixed, const MediaAvSyncStartupCapacity& capacity);
    static ::media::Result<MediaAvSyncRtpInputPlan> planRtpInputClock(
        const MediaRealtimeRtpTranscodeRequest& request,
        MediaAvSourceLifecycleMode lifecycleMode);
    static ::media::Result<MediaAvSyncPlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaTsAudioVideoSelectedProgramPlan* selectedTsProgram,
        MediaAvSynchronizationDomain domain,
        const MediaAvSyncPreparedDemuxTimestampFacts* preparedDemuxFacts,
        MediaAvSyncCapacityFacts capacity,
        const MediaRealtimeDeploymentEnvelope& deployment,
        MediaBranchMode audioBranchMode,
        std::optional<int> resolvedOutputAudioSampleRate);

private:
    MediaAvSyncPlanner() = delete;
};

} // namespace media::ffmpeg::graph
