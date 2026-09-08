#pragma once

#include "internal/graph/planner/MediaPipelineCapabilityScanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/planner/realtime/MediaRawRtpInputPreparer.h"

#include <media_transcode/Result.h>

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaAvSyncPlan;

struct MediaRealtimeRawInputPlan final {
    static constexpr int VideoStreamIndex = 0;
    static constexpr int AudioStreamIndex = 0;

    std::string videoUrl;
    std::string videoSdp;
    MediaInputVideoStreamInfo video;
    MediaRealtimeRtpTransportPlan videoTransport;
    MediaRtpDepacketizerConfig videoDepacketizer;
    MediaPreparedRtpAccessUnitEnvelope videoAccessUnitEnvelope;
    std::string audioUrl;
    std::string audioSdp;
    std::optional<MediaInputAudioStreamInfo> audio;
    std::optional<MediaRealtimeRtpTransportPlan> audioTransport;
    std::optional<MediaRtpDepacketizerConfig> audioDepacketizer;
    std::optional<MediaPreparedRtpAccessUnitEnvelope> audioAccessUnitEnvelope;
};

class MediaRealtimeInputPlanner final {
public:
    static ::media::Result<MediaRealtimeRawInputPlan> planRawRtp(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaAvSyncPlan* avSync);
    static void applyNodePlans(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeRawInputPlan* raw,
        MediaRealtimeRtpTranscodePlanningDraft& plan);
    static ::media::Result<MediaPreparedRealtimeInputScan> prepare(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaPipelinePlannerOptions& options,
        const MediaRealtimePreflightIo* io);
    static ::media::Result<MediaPreparedRawRtpProbe> prepareRawRtpVideo(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeInputPlanner() = delete;
};

} // namespace media::ffmpeg::graph
