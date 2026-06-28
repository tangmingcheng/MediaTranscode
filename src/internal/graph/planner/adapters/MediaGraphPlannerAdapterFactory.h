#pragma once

#include "internal/graph/planner/adapters/MediaGraphPlannerAdapter.h"
#include "media_transcode/Result.h"

#include <memory>

namespace media::ffmpeg::graph {

enum class MediaGraphPlannerAdapterKind {
    Local,
    Realtime
};

class MediaGraphPlannerAdapterFactory final {
public:
    static ::media::Result<std::unique_ptr<MediaGraphPlannerAdapter>> create(
        MediaGraphPlannerAdapterKind kind);
};

} // namespace media::ffmpeg::graph
