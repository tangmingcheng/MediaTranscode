#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/protocol/MediaProtocolOutputSessionKey.h"
#include "media_transcode/Result.h"

#include <vector>

namespace media::ffmpeg::graph {

// Final-topology product. Runtime binds these identities without discovering
// additional consumers or changing the frozen notification domain.
struct MediaNetworkStorageWakeupPlan final {
    MediaNodeId transportSource;
    MediaProtocolOutputSessionKey sessionKey;
    std::vector<MediaNodeId> producerTargets;
};

class MediaNetworkStorageWakeupPlanner final {
public:
    static ::media::Result<MediaNetworkStorageWakeupPlan> plan(
        const MediaGraph& graph, MediaNodeId transportSource);
};

} // namespace media::ffmpeg::graph
