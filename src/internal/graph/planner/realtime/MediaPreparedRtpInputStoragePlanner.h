#pragma once
#include "internal/graph/planner/realtime/MediaRealtimeInputPlanner.h"
namespace media::ffmpeg::graph {
struct MediaPreparedRtpInputStorageFacts final {
    MediaPreparedInputPayloadEnvelope payload;
    std::uint64_t reservedBytes;
    std::string authority;
    std::optional<MediaPreparedRtpStorageReservation> preparation;
};
class MediaPreparedRtpInputStoragePlanner final {
public:
    static ::media::Result<MediaPreparedRtpInputStorageFacts> plan(
        const MediaRealtimeRawInputPlan& raw,
        const MediaRtpIngressPlan* video, const MediaRtpIngressPlan* audio,
        const MediaPreparedRealtimeInput* preparedVideo,
        const MediaPreparedRealtimeInput* preparedAudio);
};
} // namespace media::ffmpeg::graph
