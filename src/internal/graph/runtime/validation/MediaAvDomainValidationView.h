#pragma once

#include "internal/graph/runtime/factory/MediaAvSyncRuntimeBinding.h"

#include <algorithm>
#include <span>

namespace media::ffmpeg::graph {

struct MediaAvDomainValidationView final {
    const MediaAvSyncGroupKey& groupKey;
    const MediaAvSyncPlan& plan;
    const MediaAvRuntimeDomainRole& role;
    const MediaRealtimeEdgePolicySet& edgePolicies;
    const MediaDatagramTransportPlanTemplate& datagramTransport;
    MediaSynchronizedAudioExecutionProduct audioExecutionProduct;
    const MediaAvSyncRuntimeOutputProduct& outputProduct;
    std::span<const MediaNodeId> members;

    bool contains(MediaNodeId id) const noexcept
    {
        return std::find(members.begin(), members.end(), id) != members.end();
    }
};

} // namespace media::ffmpeg::graph
