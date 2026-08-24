#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaTranscodeStreamSet.h"
#include "internal/graph/planner/realtime/MediaDatagramTransportPlan.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"

#include <cstddef>

namespace media::ffmpeg::graph {

class MediaDatagramOutputGraphShapeValidator final {
public:
    static ::media::Status validate(
        const MediaGraph& graph,
        const MediaDatagramTransportPlanTemplate& planTemplate,
        MediaProtocolOutputSessionKey sessionKey,
        MediaTranscodeStreamSet streamSet,
        MediaNodeKind materializerKind,
        std::size_t materializerCount,
        const MediaRealtimeEdgePolicySet& edgePolicies);

private:
    MediaDatagramOutputGraphShapeValidator() = delete;
};

} // namespace media::ffmpeg::graph
