#pragma once
#include "internal/graph/planner/realtime/MediaCompositionPayloadResidencePlanner.h"
#include "internal/graph/model/MediaCompositionGraphIdentity.h"
namespace media::ffmpeg::graph {
class MediaCompositionResidenceReadbackValidator final {
public:
    static ::media::Status validate(const MediaGraph& graph,const MediaCompositionGraphTopology& topology,
        const MediaCompositionGraphIdentity& identity,const MediaCompositionResidenceFacts& facts,
        const MediaCompositionReferenceResidencePlan& reference,const MediaFinalGraphReferenceStoragePlan& actual);
};
}
