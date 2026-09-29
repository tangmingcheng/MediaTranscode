#pragma once

#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/planner/audio/MediaAudioEncoderFifoRetentionPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaRealtimeAvOutputRuntimePlan final {
    MediaResolvedAudioOutputPlan audioOutput;
    MediaAvSyncGroupKey groupKey;
    MediaAvSyncPlan synchronization;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaRunningTime activationOutputLead;
    MediaAvSyncOutputAdapterKind outputAdapter;
    std::variant<MediaSeparateRtpOutputRuntimePlan,
                 MediaProjectMpegTsRuntimeOutputPlan> protocolOutput;
    MediaDatagramTransportPlanTemplate datagramTransport;
    std::optional<MediaAudioEncoderFifoRetentionPlan> encoderFifoRetention;
};

} // namespace media::ffmpeg::graph
