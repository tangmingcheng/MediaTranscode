#pragma once

#include "internal/graph/planner/realtime/MediaRtpSourcePreflight.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeCompositionSourceFactsPlanner.h"

#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include <span>
#include <stop_token>
#include <vector>

namespace media::ffmpeg::graph {

class MediaMasterClock;
class MediaPreparedRtpSourceSession;

struct MediaRealtimeCompositionCapturedSource final {
    MediaRtpSourcePreflight capture;
    MediaRtpSourceObservation observation;
    std::chrono::steady_clock::time_point deadline;
    MediaRealtimeCompositionSourceFacts facts;
    std::shared_ptr<MediaPreparedRtpSourceSession> session;
    std::shared_ptr<MediaMasterClock> masterClock;
    std::optional<MediaVideoSourcePlan> sessionVideoPlan;
};

// Bounded parallel preparation workers finish before this transaction returns.
// Successful capture owners remain live until the composition transaction seals
// them; failure cancels and joins every worker before releasing any owner.
class MediaRealtimeCompositionInputPreparer final {
public:
    static ::media::Result<std::vector<MediaRealtimeCompositionCapturedSource>> prepare(
        const MediaRealtimeRtpTranscodeRequest& request,
        std::span<const MediaRealtimeCompositionInputPlan> inputs,
        const std::shared_ptr<MediaMasterClock>& masterClock,
        std::stop_token cancellation,
        const MediaSharedPreparationStorage& storage);
};

} // namespace media::ffmpeg::graph
