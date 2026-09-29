#pragma once

#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

#include <variant>

namespace media::ffmpeg::graph {

// Borrowed only for synchronous graph construction; segments retain no references.
struct MediaRealtimeAvOutputSegmentPlan final {
    const MediaAvSyncGroupKey& groupKey;
    const MediaRealtimeEdgePolicySet& edgePolicies;
    MediaBranchMode audioBranchMode;
    MediaAvSyncOutputAdapterKind outputAdapter;
    const std::variant<MediaSeparateRtpOutputRuntimePlan,
                       MediaProjectMpegTsRuntimeOutputPlan>& protocolOutput;
    const MediaDatagramTransportPlanTemplate& datagramTransport;
};

} // namespace media::ffmpeg::graph
