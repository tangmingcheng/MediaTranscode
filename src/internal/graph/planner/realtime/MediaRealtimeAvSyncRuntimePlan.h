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
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include "internal/graph/planner/audio/MediaAudioEncoderFifoRetentionPlan.h"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaAudioCorrectionReachabilityPlan final {
    int outputSampleRate;
    std::int64_t epochOutputSampleIndex;
    std::int64_t worstCaseInFlightSamples;
    std::int64_t mailboxDeliveryMarginSamples;
    std::int64_t maximumResamplerOutputBlockSamples;
    std::int64_t commandLeadSamples;
    std::size_t mailboxCapacity;
    friend bool operator==(const MediaAudioCorrectionReachabilityPlan&,
                           const MediaAudioCorrectionReachabilityPlan&) = default;
};

struct MediaRealtimeAvSourceRuntimePlan {
    MediaAudioPipelinePlan audioPipeline;
    std::optional<MediaRealtimeRtpInputNodePlan> isolatedAudioInput;
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan synchronization;
    MediaRealtimeAvSyncAssemblyPlan assembly;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
    MediaRunningTime activationOutputLead;
    bool videoFilterActive;
    MediaAvGenerationTransitionPlan transition;
    std::optional<int> inputAudioSampleRate;
    std::optional<MediaAudioCorrectionReachabilityPlan> audioCorrection;
};

struct MediaRealtimeAvSyncRuntimePlan final : MediaRealtimeAvSourceRuntimePlan {
    MediaRealtimeAvSyncComponentBounds componentBounds;
    MediaRealtimeAvSyncPlanningFacts planningFacts;
    MediaAvSyncOutputAdapterKind outputAdapter;
    std::variant<MediaSeparateRtpOutputRuntimePlan,
                 MediaProjectMpegTsRuntimeOutputPlan> protocolOutput;
    MediaDatagramTransportPlanTemplate datagramTransport;
    std::optional<MediaAudioEncoderFifoRetentionPlan> encoderFifoRetention;
};

} // namespace media::ffmpeg::graph
