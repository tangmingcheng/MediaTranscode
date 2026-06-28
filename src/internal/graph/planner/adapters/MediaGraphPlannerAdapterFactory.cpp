#include "internal/graph/planner/adapters/MediaGraphPlannerAdapterFactory.h"

#include "internal/graph/planner/adapters/LocalGraphPlannerAdapter.h"
#include "internal/graph/planner/adapters/RealtimeGraphPlannerAdapter.h"

namespace media::ffmpeg::graph {

::media::Result<std::unique_ptr<MediaGraphPlannerAdapter>> MediaGraphPlannerAdapterFactory::create(
    MediaGraphPlannerAdapterKind kind)
{
    switch (kind) {
    case MediaGraphPlannerAdapterKind::Local:
        return ::media::Result<std::unique_ptr<MediaGraphPlannerAdapter>>::success(
            std::make_unique<LocalGraphPlannerAdapter>());

    case MediaGraphPlannerAdapterKind::Realtime:
        return ::media::Result<std::unique_ptr<MediaGraphPlannerAdapter>>::success(
            std::make_unique<RealtimeGraphPlannerAdapter>());
    }

    return ::media::Result<std::unique_ptr<MediaGraphPlannerAdapter>>::failure(
        ::media::ErrorInfo::unsupported("MediaGraphPlannerAdapterFactory unsupported adapter kind"));
}

} // namespace media::ffmpeg::graph
