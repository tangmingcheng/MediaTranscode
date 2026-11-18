#include "internal/graph/nodes/sync/MediaAvScheduledOutputBuilder.h"

namespace media::ffmpeg::graph {
::media::Result<MediaBufferRef> MediaAvScheduledOutputBuilder::canonical(
    const MediaCanonicalAccessUnitBuffer& unit,
    MediaRunningTime presentationOnMaster,
    MediaRunningTime dispatchOnMaster,
    MediaRunningTime emitOnMaster)
{
    auto dispatch = unit.canonicalDispatch();
    if (!dispatch) return ::media::Result<MediaBufferRef>::failure(dispatch.error());
    return MediaScheduledAccessUnit::create(MediaScheduledAccessUnitParameters{
        unit.media(), unit.stream(), unit.canonicalPresentation(), dispatch.value(),
        presentationOnMaster, dispatchOnMaster, emitOnMaster,
        unit.canonicalDuration(), unit.generation(), unit.canonicalSequence(),
        unit.codecReceiveObservation()});
}
} // namespace media::ffmpeg::graph
