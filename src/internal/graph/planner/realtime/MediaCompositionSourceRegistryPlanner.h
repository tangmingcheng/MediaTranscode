#pragma once
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
namespace media::ffmpeg::graph {
class MediaCompositionSourceRegistryPlanner final {
public:
    static ::media::Result<std::uint64_t> plan(const MediaVideoSourcePlan& video);
};
}
