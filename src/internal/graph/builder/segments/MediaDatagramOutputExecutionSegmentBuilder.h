#pragma once

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
    MediaNodeId shaper;
    MediaNodeId sender;
};

class MediaDatagramOutputExecutionSegmentBuilder final {
public:
    static ::media::Result<MediaDatagramOutputExecutionSegmentResult> build(
        MediaGraph& graph,
        const MediaDatagramOutputExecutionSegmentOptions& options);
    static ::media::Status connectWireSource(
        MediaGraph& graph,
        const MediaDatagramOutputExecutionSegmentResult& execution,
        MediaEndpoint wireSource,
        const MediaEdgePolicy& policy,
        const char* label);
    static ::media::Status connectTransportConsumer(
        MediaGraph& graph,
        const MediaDatagramOutputExecutionSegmentResult& execution,
        MediaNodeId consumer,
        const char* port,
        const MediaEdgePolicy& policy,
        const char* label);

private:
    MediaDatagramOutputExecutionSegmentBuilder() = delete;
};

} // namespace media::ffmpeg::graph
