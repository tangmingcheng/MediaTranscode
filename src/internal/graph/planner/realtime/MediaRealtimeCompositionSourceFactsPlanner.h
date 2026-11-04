#pragma once

#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"

#include <cstddef>

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionInputPlan;
struct MediaRtpSourceObservation;

struct MediaRealtimeCompositionSourceFacts final {
    MediaRealtimeRtpTranscodeRequest request;
    MediaRealtimeRawInputPlan raw;
    MediaVideoColorRangeFact color;
    MediaVideoSourceTiming timing;
};

class MediaRealtimeCompositionSourceFactsPlanner final {
public:
    static ::media::Result<MediaRealtimeCompositionSourceFacts> plan(
        const MediaRealtimeRtpTranscodeRequest& request, std::size_t index,
        const MediaRealtimeCompositionInputPlan& input,
        const MediaRtpSourceObservation& observed);

private:
    MediaRealtimeCompositionSourceFactsPlanner() = delete;
};

} // namespace media::ffmpeg::graph
