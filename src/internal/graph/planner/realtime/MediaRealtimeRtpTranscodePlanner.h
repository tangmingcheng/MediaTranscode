#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputStreamInfo;

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
    bool writePacingEnabled = false;
    int64_t writePacingBytesPerSecond = 0;
    int64_t writePacingBurstBytes = 0;
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
    MediaLatencyPolicy pacingPolicy;
    bool monotonicPacketTimestamps = false;
    int startupDelayMs = 0;
};

struct MediaRealtimeAvStartBarrierPlan {
    bool expectVideo = false;
    bool expectAudio = false;
    bool requireVideoKeyFrame = false;
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
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
    bool videoInputStartRequiresKeyFrame = false;
    MediaRealtimeRtpInputNodePlan input;
    MediaRealtimeRtpInputNodePlan audioInput;
    bool useIsolatedAudioInput = false;
    MediaRealtimeRtpOutputNodePlan videoOutput;
    MediaRealtimeRtpOutputNodePlan audioOutput;
    MediaRealtimeMuxedOutputPlan muxedOutput;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan videoMux;
    MediaRealtimeMuxNodePlan audioMux;
    MediaRealtimeAvStartBarrierPlan avStartBarrier;
    std::optional<MediaAvSyncPlan> avSync;
};

struct MediaRealtimeTranscodePreflight final {
    MediaRealtimeRtpTranscodePlan plan;
    std::optional<MediaPreparedRealtimeInput> prepared;
};

class MediaRealtimeRtpTranscodePlanner final {
public:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> plan(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflight(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflight(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputOpener& opener);
    static ::media::Status validateRealtimeRequestNoIo(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> planWithInput(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputStreamInfo* inputInfo);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflightImpl(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputOpener* opener);
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
