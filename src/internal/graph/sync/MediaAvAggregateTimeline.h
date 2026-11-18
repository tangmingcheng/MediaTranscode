#pragma once

#include "internal/graph/planner/realtime/MediaAvContinuousAggregatePlan.h"
#include "internal/graph/sync/MediaPlaybackEpoch.h"

#include <limits>

namespace media::ffmpeg::graph {

// Pure conversions shared by the independently scheduled output grids.
// The planner validates the rates and nonnegative initial audio sample.
// This class owns no cursor, activation state, or scheduling policy.
class MediaAvAggregateTimeline final {
public:
    struct SourceInterval {
        MediaRunningTime begin;
        MediaRunningTime end;
    };

    static ::media::Result<SourceInterval> sourceInterval(
        const MediaPlaybackEpoch& epoch, MediaRunningTime presentation,
        MediaRunningTime duration)
    {
        using Result = ::media::Result<SourceInterval>;
        auto relative = presentation.checkedSubtract(epoch.sourceStart);
        if (!relative) return Result::failure(relative.error());
        auto begin = epoch.masterRelease.checkedAdd(relative.value());
        if (!begin) return Result::failure(begin.error());
        auto end = begin.value().checkedAdd(duration);
        if (!end) return Result::failure(end.error());
        return Result::success({begin.value(), end.value()});
    }

    static ::media::Result<MediaRunningTime> videoPresentation(
        const MediaAvContinuousAggregateTopology& plan, std::int64_t frame)
    {
        return MediaRunningTime::checkedFromTicks(frame,
            plan.videoFrameRate.den, plan.videoFrameRate.num);
    }

    static ::media::Result<MediaRunningTime> audioPresentation(
        const MediaAvContinuousAggregateTopology& plan, std::int64_t sample)
    {
        if (sample < std::numeric_limits<std::int64_t>::min() + plan.initialAudioSample)
            return ::media::Result<MediaRunningTime>::failure(::media::ErrorInfo::invalidArgument(
                "Aggregate output sample offset is not representable"));
        return MediaRunningTime::checkedFromTicks(
            sample - plan.initialAudioSample, 1, plan.audio.sampleRate());
    }

    static ::media::Result<MediaRunningTime> presentationMaster(
        const MediaPlaybackEpoch& epoch, MediaRunningTime presentation)
    {
        return epoch.masterRelease.checkedAdd(presentation);
    }
};

} // namespace media::ffmpeg::graph
