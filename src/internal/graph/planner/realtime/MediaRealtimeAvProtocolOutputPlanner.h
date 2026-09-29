#pragma once

#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/planner/avsync/MediaAvSyncOutputAdapterKind.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/planner/realtime/MediaPreparedEmissionResolver.h"
#include "internal/graph/planner/realtime/MediaRealtimeDeploymentEnvelope.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPlanningDraft.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/sync/MediaAvSyncGroupKey.h"

namespace media::ffmpeg::graph {

// Output facts only: no compressed source, decoder or input clock authority.
struct MediaRealtimeAvProtocolOutputRequest final {
    const MediaAvSyncGroupKey& groupKey;
    RealtimeOutputStreamLayout layout;
    MediaOutputTransportKind transport;
    const std::optional<MediaAvSyncRtpOutputPlan>& rtp;
    const std::optional<MediaAvSyncProjectMpegTsOutputPlan>& mpegTs;
    std::optional<MediaRunningTime> outputLead;
    MediaRational videoFrameRate;
    std::optional<int> audioSampleRate;
    std::optional<std::int64_t> audioBatchSamples;
    std::uint64_t maximumQueuedPacketBytes;
    const MediaRealtimeDeploymentEnvelope& deployment;
    const MediaPreparedRealtimeEmissionSet& emission;
};

struct MediaRealtimeAvProtocolOutputPlan final {
    MediaAvSyncOutputAdapterKind adapter;
    MediaRunningTime activationOutputLead;
    std::variant<MediaSeparateRtpOutputRuntimePlan,
                 MediaProjectMpegTsRuntimeOutputPlan> protocolOutput;
    MediaDatagramTransportPlanTemplate datagramTransport;
};

class MediaRealtimeAvProtocolOutputPlanner final {
public:
    static ::media::Result<MediaRealtimeAvProtocolOutputPlan> plan(
        const MediaRealtimeAvProtocolOutputRequest& request,
        MediaRealtimeOutputPlanningDraft& output);
private:
    MediaRealtimeAvProtocolOutputPlanner() = delete;
};

} // namespace media::ffmpeg::graph
