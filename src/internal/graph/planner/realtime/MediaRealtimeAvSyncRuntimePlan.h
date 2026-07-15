#pragma once

#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/avsync/MediaAvGenerationTransitionPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/planner/realtime/MediaProjectMpegTsOutputPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaScheduledRtpOutputPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <cstdint>
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

struct MediaSeparateRtpOutputRuntimePlan final {
    MediaScheduledRtpOutputPlan video;
    MediaScheduledRtpOutputPlan audio;
    std::string sdpPath;
};

struct MediaProjectMpegTsRuntimeOutputPlan final {
    std::string url;
    MediaOutputResourceKind resourceKind;
    MediaMuxSessionKind muxSessionKind;
    MediaProjectMpegTsOutputPlan protocol;
};

struct MediaRealtimeAvSyncRuntimePlan final {
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan synchronization;
    MediaAvSyncOutputAdapterKind outputAdapter;
    std::variant<MediaSeparateRtpOutputRuntimePlan,
                 MediaProjectMpegTsRuntimeOutputPlan> protocolOutput;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
    MediaAvGenerationTransitionPlan transition;
    MediaRealtimeAvSyncPlanningFacts planningFacts;
    MediaAudioCorrectionReachabilityPlan audioCorrection;
};

} // namespace media::ffmpeg::graph
