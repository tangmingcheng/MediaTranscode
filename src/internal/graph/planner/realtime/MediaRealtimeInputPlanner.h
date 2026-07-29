#pragma once

#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"

#include <media_transcode/Result.h>

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAvSyncPlan;

struct MediaRealtimeRawInputPlan final {
    std::string videoUrl;
    std::string videoSdp;
    MediaInputVideoStreamInfo video;
    MediaRealtimeRtpTransportPlan videoTransport;
    MediaRtpDepacketizerConfig videoDepacketizer;
    std::string audioUrl;
    std::string audioSdp;
    std::optional<MediaInputAudioStreamInfo> audio;
    std::optional<MediaRealtimeRtpTransportPlan> audioTransport;
    std::optional<MediaRtpDepacketizerConfig> audioDepacketizer;
};

class MediaRealtimeInputPlanner final {
public:
    static ::media::Result<MediaRealtimeRawInputPlan> planRawRtp(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaAvSyncPlan* avSync);
    static void applyNodePlans(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeRawInputPlan* raw,
        MediaRealtimeRtpTranscodePlan& plan);
    static ::media::Result<MediaPreparedRealtimeInputScan> prepare(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaPipelinePlannerOptions& options,
        const MediaRealtimePreflightIo* io);

private:
    MediaRealtimeInputPlanner() = delete;
};

} // namespace media::ffmpeg::graph
