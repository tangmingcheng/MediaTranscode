#pragma once

#include "internal/graph/planner/realtime/MediaFinalGraphResourceLedgerCompiler.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeInitialOutputResourcePartition final {
    MediaFinalGraphResourceLedger shared;
    MediaFinalGraphResourceLedger encoding;
    std::uint64_t protocolFixedStorageBytes;
};

class MediaRealtimeInitialOutputResourcePartitionPlanner final {
public:
    static ::media::Result<MediaRealtimeInitialOutputResourcePartition> plan(
        const MediaGraph& graph,
        const MediaRealtimeGraphResourceLedgerPlan& ledger,
        const MediaRealtimeInitialVideoOutputTopology& topology);
};

} // namespace media::ffmpeg::graph
