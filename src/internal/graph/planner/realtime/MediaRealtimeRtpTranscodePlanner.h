#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputStreamInfo;
struct MediaTsSelectedProgramPlan;

struct MediaRealtimeRtpTransportPlan final {
    MediaIpAddressFamily addressFamily;
    std::string bindAddress;
    uint16_t rtpPort;
    uint16_t rtcpPort;
    uint8_t payloadType;
    int clockRate;
    int receiveBufferBytes;
    int maximumDatagramBytes;
    std::size_t reorderWindowPackets;
    int maximumReorderDelayMs;
    int cancellableReadTimeoutMs;
    bool requireSenderReports;
    bool requireCname;
    int senderReportTimeoutMs;
    int cnameTimeoutMs;
    std::optional<MediaRtcpCompositionMode> rtcpCompositionMode;
};

struct MediaRealtimeTsInputPlan final {
    std::string demuxFormat;
    std::size_t packetSize = 0;
    std::size_t avioBufferBytes = 0;
    std::size_t maximumDatagramBytes = 0;
    std::size_t evidenceTimelineCapacity = 0;
    std::uint64_t maximumPacketPositionRegressionBytes = 0;

    static ::media::Result<MediaRealtimeTsInputPlan> create(
        std::size_t packetSize,
        std::uint64_t probeWindowBytes,
        std::uint64_t maximumPacketPositionRegressionBytes,
        std::size_t evidenceTimelineCapacity);
    static ::media::Result<std::size_t> minimumEvidenceCapacity(
        std::size_t packetSize,
        std::uint64_t probeWindowBytes,
        std::uint64_t maximumPacketPositionRegressionBytes);
};

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
    std::optional<MediaRealtimeRtpTransportPlan> rtpTransport;
    std::optional<MediaRtpDepacketizerConfig> rtpDepacketizer;
    std::optional<MediaRealtimeTsInputPlan> mpegTs;
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
    bool videoPacketCopyNormalizationRequired;
    bool audioPacketNormalizationRequired;
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
        const MediaRealtimePreflightIo& io);
    static ::media::Status validateRealtimeRequestNoIo(
        const MediaRealtimeRtpTranscodeRequest& request);

private:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> planWithInput(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputStreamInfo* inputInfo,
        const MediaTsSelectedProgramPlan* selectedTsProgram = nullptr);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflightImpl(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimePreflightIo* io);
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
