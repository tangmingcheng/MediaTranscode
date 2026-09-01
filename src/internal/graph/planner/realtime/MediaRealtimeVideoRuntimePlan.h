#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeProtocolOutputPlan.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"
#include "internal/graph/time/MediaRunningTime.h"

#include <cstddef>
#include <cstdint>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaRealtimeVideoStartupPlan final {
    bool requireKeyFrame;
    MediaRunningTime maximumWait;
    std::size_t packetCapacity;
    std::uint64_t maximumUnitBytes;
    std::uint64_t byteCapacity;
};

enum class MediaRealtimeVideoPacketTimingMode {
    PacketDuration,
    PlannedCadence
};

struct MediaRealtimeVideoTimingPlan final {
    MediaRational sourceTimeBase;
    MediaRational outputFrameRate;
    MediaRational scheduledPacketTimeBase;
    MediaRealtimeVideoPacketTimingMode packetTimingMode;
};

struct MediaRealtimeVideoSchedulingPlan final {
    bool pacingEnabled;
    MediaRunningTime activationLead;
    MediaRunningTime transportLead;
    MediaRunningTime protocolPreparationLead;
    std::uint64_t initialGeneration;
};

using MediaRealtimeVideoOutputAdapterPlan = std::variant<
    MediaVideoOnlySeparateRtpOutputRuntimePlan,
    MediaProjectMpegTsRuntimeOutputPlan>;

struct MediaRealtimeVideoRuntimePlan final {
    MediaRealtimeVideoStartupPlan startup;
    MediaRealtimeVideoTimingPlan timing;
    MediaRealtimeVideoSchedulingPlan scheduling;
    MediaProtocolOutputSessionKey sessionKey;
    bool packetCopyNormalizationRequired;
    MediaRealtimeVideoOutputAdapterPlan outputAdapter;
    MediaDatagramTransportPlanTemplate datagramTransport;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaVideoLineageEdgePolicySet lineageEdgePolicies;
    MediaThreadingPolicy threadingPolicy;
};

using MediaRealtimeRuntimePlan = std::variant<
    MediaRealtimeVideoRuntimePlan,
    MediaRealtimeAvSyncRuntimePlan>;

} // namespace media::ffmpeg::graph
