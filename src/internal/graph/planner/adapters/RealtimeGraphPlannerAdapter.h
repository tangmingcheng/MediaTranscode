#pragma once

#include "internal/graph/planner/adapters/MediaGraphPlannerAdapter.h"

namespace media::ffmpeg::graph {

class RealtimeGraphPlannerAdapter final : public MediaGraphPlannerAdapter {
public:
    const char* name() const noexcept override;

    ::media::Result<MediaGraphPlannerAdapterResult> plan(
        const MediaGraph& graph,
        const MediaGraphPlannerAdapterOptions& options = {}) const override;

private:
    static MediaGraphClusterTopology buildTopology(const MediaGraphPlannerAdapterOptions& options);
    static MediaGraphPlanningPolicy buildPolicy(const MediaGraphPlannerAdapterOptions& options);
};

} // namespace media::ffmpeg::graph
