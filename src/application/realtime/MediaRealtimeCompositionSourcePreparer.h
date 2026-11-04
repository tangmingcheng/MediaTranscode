#pragma once
#include "internal/graph/runtime/ffmpeg/MediaVideoFrameReadiness.h"

#include "application/realtime/MediaRealtimeCompositionInputPreparer.h"
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
#include "internal/graph/planner/realtime/MediaRtpSourceFirstFrameProbe.h"

namespace media::ffmpeg::graph {

struct MediaRealtimeCompositionPreparedSourceVideo final {
    MediaVideoSourcePlan plan;
    std::unique_ptr<FFmpegFormatContextBuffer> descriptor;
    MediaRtpSourceFirstFrameEvidence evidence;
    ::media::ffmpeg::FramePtr tile;
    std::optional<MediaVideoFrameReadiness> tileReadiness;
};

class MediaRealtimeCompositionSourcePreparer final {
public:
    static ::media::Result<MediaRealtimeCompositionPreparedSourceVideo> prepare(
        MediaRealtimeCompositionCapturedSource& captured,
        const MediaRealtimeCompositionSourceFacts& facts,
        MediaVideoSourcePlan candidate,
        const MediaHardwareDescriptor& target,
        AVBufferRef* device, std::stop_token cancellation,
        const MediaSharedPreparationStorage& storageScope);
};

} // namespace media::ffmpeg::graph
