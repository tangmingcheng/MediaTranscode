#pragma once
#include "internal/graph/builder/segments/MediaOutputSegmentBindings.h"

#include "internal/graph/builder/MediaEndpoint.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaDatagramOutputExecutionSegmentOptions final {
    std::string prefix;
    MediaEndpoint activation;
    MediaProtocolOutputSessionKey sessionKey;
    MediaTranscodeStreamSet streamSet;
    const MediaDatagramTransportPlanTemplate* transportPlan;
    const MediaRealtimeEdgePolicySet* edgePolicies;
};

struct MediaDatagramOutputExecutionSegmentResult final {
    MediaNodeId transportPlanSource;
    MediaNodeId sender;
    MediaOutputSegmentBindings bindings;
};

class MediaDatagramOutputExecutionSegmentBuilder final {
public:
    static ::media::Status validateSessionAvailable(
        const MediaGraph& graph,
        const MediaProtocolOutputSessionKey& sessionKey);
    static ::media::Result<MediaDatagramOutputExecutionSegmentResult> build(
        MediaGraph& graph,
        const MediaDatagramOutputExecutionSegmentOptions& options);

private:
    MediaDatagramOutputExecutionSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
