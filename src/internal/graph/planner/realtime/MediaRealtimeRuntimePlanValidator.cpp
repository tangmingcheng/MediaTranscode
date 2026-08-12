#include "internal/graph/planner/realtime/MediaRealtimeRuntimePlanValidator.h"

#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlanValidator.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeVideoRuntimePlanValidator.h"

#include <type_traits>
#include <variant>

namespace media::ffmpeg::graph {

::media::Status MediaRealtimeRuntimePlanValidator::validate(
    const MediaRealtimeRtpTranscodePlan& plan)
{
    return std::visit(
        [&plan](const auto& runtime) -> ::media::Status {
            using Runtime = std::decay_t<decltype(runtime)>;
            if constexpr (std::is_same_v<
                              Runtime,
                              MediaRealtimeVideoRuntimePlan>) {
                return MediaRealtimeVideoRuntimePlanValidator::validate(
                    plan, runtime);
            } else {
                return MediaRealtimeAvSyncRuntimePlanValidator::validate(
                    plan, runtime);
            }
        },
        plan.runtime);
}

} // namespace media::ffmpeg::graph
