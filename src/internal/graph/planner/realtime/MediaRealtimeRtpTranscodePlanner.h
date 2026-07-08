#pragma once

#include "internal/graph/core/MediaEdge.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct MediaRealtimeRtpInputNodePlan {
    std::string url;
    std::string sdpText;
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

struct MediaRealtimeMuxedOutputPlan {
    std::string url;
    std::string format;
    std::string mediaId;
};

struct MediaRealtimeSdpWriterPlan {
    std::string path;
    std::string mediaId;
    int expectedContexts = 1;
};

struct MediaRealtimeMuxNodePlan {
    bool expectVideo;
    bool expectAudio;
};

struct MediaRealtimeRtpTranscodePlan {
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    RealtimeOutputStreamLayout outputLayout;
    MediaPipelinePlan videoPlan;
    MediaAudioPipelinePlan audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    MediaAudioTranscodeParameters audioParameters;
    MediaGraphQueueParameters queues;
    MediaEdgePolicy edgePolicy;
    MediaRealtimeRtpInputNodePlan input;
    MediaRealtimeRtpOutputNodePlan videoOutput;
    MediaRealtimeRtpOutputNodePlan audioOutput;
    MediaRealtimeMuxedOutputPlan muxedOutput;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan videoMux;
    MediaRealtimeMuxNodePlan audioMux;
};

class MediaRealtimeRtpTranscodePlanner final {
public:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
