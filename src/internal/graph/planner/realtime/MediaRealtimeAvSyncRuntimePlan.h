#pragma once

#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncAssemblyPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanningProducts.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaAudioCorrectionReachabilityPlan final {
    int outputSampleRate;
    std::int64_t epochOutputSampleIndex;
    std::int64_t worstCaseInFlightSamples;
    std::int64_t protocolBatchSamples;
    std::int64_t mailboxDeliveryMarginSamples;
    std::int64_t maximumResamplerOutputBlockSamples;
    std::int64_t commandLeadSamples;
    std::size_t mailboxCapacity;
    friend bool operator==(const MediaAudioCorrectionReachabilityPlan&,
                           const MediaAudioCorrectionReachabilityPlan&) = default;
};

struct MediaRealtimeAvSyncRuntimePlan final {
    MediaAudioPipelinePlan audioPipeline;
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput;
    MediaRealtimeAvSyncComponentBounds componentBounds;
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan synchronization;
    MediaRealtimeAvSyncAssemblyPlan assembly;
    MediaAvSyncOutputAdapterKind outputAdapter;
    std::variant<MediaSeparateRtpOutputRuntimePlan,
                 MediaProjectMpegTsRuntimeOutputPlan> protocolOutput;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
    MediaRunningTime activationOutputLead;
    bool videoFilterActive;
    MediaAvGenerationTransitionPlan transition;
    MediaRealtimeAvSyncPlanningFacts planningFacts;
    std::optional<MediaAudioCorrectionReachabilityPlan> audioCorrection;
};

} // namespace media::ffmpeg::graph
