#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/planner/adapters/MediaGraphPlannerAdapterOptions.h"
#include "internal/graph/planner/adapters/MediaGraphPlannerAdapterResult.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphPlannerAdapter {
public:
    virtual ~MediaGraphPlannerAdapter() = default;

    virtual const char* name() const noexcept = 0;

    virtual ::media::Result<MediaGraphPlannerAdapterResult> plan(
        const MediaGraph& graph,
        const MediaGraphPlannerAdapterOptions& options = {}) const = 0;

protected:
    MediaGraphPlannerAdapter() = default;
};

} // namespace media::ffmpeg::graph
