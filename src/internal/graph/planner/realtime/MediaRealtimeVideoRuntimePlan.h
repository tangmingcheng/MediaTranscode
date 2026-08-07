#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeOutputPlanningDraft.h"
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

enum class MediaRealtimeVideoTimestampAuthority {
    DecodeTimestamp
};

enum class MediaRealtimeVideoPacketTimingMode {
    SourceTimeBase,
    OutputCadenceTimeBase
};

struct MediaRealtimeVideoTimingPlan final {
    MediaRational sourceTimeBase;
    MediaRational outputFrameRate;
    MediaRational scheduledPacketTimeBase;
    MediaRealtimeVideoPacketTimingMode packetTimingMode;
    MediaRealtimeVideoTimestampAuthority timestampAuthority;
};

struct MediaRealtimeVideoSchedulingPlan final {
    bool pacingEnabled;
    MediaRunningTime transportLead;
};

struct MediaRealtimeVideoSeparateRtpAdapterPlan final {
    bool packetCopyNormalizationRequired;
    MediaRealtimeRtpOutputNodePlan output;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan mux;
};

struct MediaRealtimeVideoMuxedAdapterPlan final {
    bool packetCopyNormalizationRequired;
    MediaRealtimeMuxedOutputPlan output;
    MediaRealtimeMuxNodePlan mux;
};

using MediaRealtimeVideoOutputAdapterPlan = std::variant<
    MediaRealtimeVideoSeparateRtpAdapterPlan,
    MediaRealtimeVideoMuxedAdapterPlan>;

struct MediaRealtimeVideoRuntimePlan final {
    MediaRealtimeVideoStartupPlan startup;
    MediaRealtimeVideoTimingPlan timing;
    MediaRealtimeVideoSchedulingPlan scheduling;
    MediaRealtimeVideoOutputAdapterPlan outputAdapter;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
};

using MediaRealtimeRuntimePlan = std::variant<
    MediaRealtimeVideoRuntimePlan,
    MediaRealtimeAvSyncRuntimePlan>;

} // namespace media::ffmpeg::graph
