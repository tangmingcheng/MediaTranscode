#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpInputNodePlan {
    std::string url;
    std::string rtspTransport;
    int openTimeoutMs;
    int readTimeoutMs;
    int analyzeDurationUs;
    int probeSizeBytes;
    bool lowLatency;
    std::string mediaId;
};

struct MediaRealtimeRtpOutputNodePlan {
    std::string url;
    int packetSize;
    std::string mediaId;
};

struct MediaRealtimeSdpWriterPlan {
    std::string path;
    std::string mediaId;
};

struct MediaRealtimeMuxNodePlan {
    bool expectVideo;
    bool expectAudio;
};

struct MediaRealtimeRtpTranscodePlan {
    MediaRealtimeInputKind inputKind;
    MediaPipelinePlan videoPlan;
    MediaVideoTranscodeParameters videoParameters;
    MediaGraphQueueParameters queues;
    MediaEdgePolicy edgePolicy;
    MediaRealtimeRtpInputNodePlan input;
    MediaRealtimeRtpOutputNodePlan output;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan mux;
};

class MediaRealtimeRtpTranscodePlanner final {
public:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
