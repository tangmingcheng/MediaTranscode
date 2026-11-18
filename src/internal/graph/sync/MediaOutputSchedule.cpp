#include "internal/graph/sync/MediaOutputSchedule.h"

namespace media::ffmpeg::graph {

::media::Result<MediaRunningTime> MediaOutputSchedule::releaseTime(
    MediaRunningTime emit, MediaRunningTime protocolPreparationLead)
{
    if (protocolPreparationLead.nanoseconds() <= 0)
        return ::media::Result<MediaRunningTime>::failure(
            ::media::ErrorInfo::invalidArgument("Encoded output requires positive protocol preparation lead"));
    return emit.checkedSubtract(protocolPreparationLead);
}

::media::Result<MediaOutputSchedule> MediaOutputSchedule::create(
    MediaRunningTime presentation,
    MediaRunningTime dispatch,
    MediaRunningTime transportLead)
{
    if (transportLead < MediaRunningTime::fromNanoseconds(0)) {
        return ::media::Result<MediaOutputSchedule>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Output schedule requires a non-negative transport lead"));
    }
    auto emit = dispatch.checkedSubtract(transportLead);
    if (!emit) {
        return ::media::Result<MediaOutputSchedule>::failure(emit.error());
    }
    return ::media::Result<MediaOutputSchedule>::success(
        MediaOutputSchedule{presentation, dispatch, emit.value()});
}

} // namespace media::ffmpeg::graph
