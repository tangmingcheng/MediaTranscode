#include "internal/graph/planner/realtime/MediaRealtimeMediaCapacityPlanner.h"

#include "internal/graph/planner/realtime/MediaRealtimeGraphResourceLedgerPlanner.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRealtimeMediaCapacityPlan>
MediaRealtimeMediaCapacityPlanner::plan(
    const MediaRealtimeGraphResourceLedgerPlan& ledger)
{
    if (ledger.admittedGraphBytes == 0 || ledger.entries.empty() ||
        ledger.media.videoUnits == 0 || ledger.media.videoUnitBytes == 0 ||
        ledger.media.videoBytes == 0) {
        return ::media::Result<MediaRealtimeMediaCapacityPlan>::failure(
            ::media::ErrorInfo::notInitialized(
                "realtime media capacity requires an admitted graph resource ledger"));
    }
    return ::media::Result<MediaRealtimeMediaCapacityPlan>::success(
        ledger.media);
}

} // namespace media::ffmpeg::graph
