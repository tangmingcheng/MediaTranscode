#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/runtime/network/MediaDatagramServiceScopeArbiter.h"
#include "internal/graph/protocol/MediaProtocolOutputRuntimeAuthority.h"

namespace media::ffmpeg::graph {

struct MediaDatagramServiceScopeBinding final {
    MediaNodeId sender;
    std::shared_ptr<MediaDatagramServiceScopeArbiter> arbiter;
};

class MediaDatagramServiceScopeAssembler final {
public:
    static ::media::Result<std::vector<MediaDatagramServiceScopeBinding>> assemble(
        const MediaGraph& graph,
        const std::shared_ptr<MediaProtocolOutputRuntimeAuthority>& authority);
};

} // namespace media::ffmpeg::graph
