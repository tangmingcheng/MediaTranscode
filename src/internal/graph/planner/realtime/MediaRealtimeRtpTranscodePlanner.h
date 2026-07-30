#pragma once

#include "internal/graph/model/MediaLatencyPolicy.h"
#include "internal/graph/model/MediaRealtimeEdgePolicySet.h"
#include "internal/graph/model/MediaIpAddressFamily.h"
#include "internal/graph/model/MediaThreadingPolicy.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncRuntimePlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeAvSyncPlanningFacts.h"
#include "internal/graph/planner/realtime/MediaScheduledRtpPacketizationPlan.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodeRequest.h"
#include "internal/graph/planner/realtime/MediaPreparedRealtimeInput.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketOriginPolicy.h"
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
    std::size_t pesProvenanceCapacity = 0;
    MediaTsPacketOriginPolicy packetOriginPolicy;
    int programNumber = 0;
    int programMapPid = 0;
    int videoPid = 0;
    int audioPid = 0;
    int pcrPid = 0;
    std::int64_t maximumPcrGap27Mhz = 0;
    std::size_t projectionCapacity = 0;
    std::size_t initialAcquiringVideoPacketCapacity = 0;
    std::size_t initialAcquiringAudioPacketCapacity = 0;
    std::uint64_t initialAcquiringVideoByteCapacity = 0;
    std::uint64_t initialAcquiringAudioByteCapacity = 0;
    std::uint64_t maximumAcquiringVideoPacketBytes = 0;
    std::uint64_t maximumAcquiringAudioPacketBytes = 0;
    int timestampTimeBaseNumerator = 0;
    int timestampTimeBaseDenominator = 0;
    std::uint64_t initialSourceGeneration = 0;
    std::uint64_t initialRawTransportGeneration = 0;
    std::optional<MediaTsPacketDurationEvidence> videoPacketDuration;
    std::optional<MediaTsPacketDurationEvidence> audioPacketDuration;

    static ::media::Result<MediaRealtimeTsInputPlan> create(
        std::size_t packetSize,
        std::uint64_t probeWindowBytes,
        std::uint64_t maximumPacketPositionRegressionBytes,
        std::size_t evidenceTimelineCapacity,
        std::size_t selectedStreamCount);
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
    std::optional<MediaRtpUdpSenderConfig> scheduledTransport;
    std::optional<MediaScheduledRtpPacketizationPlan> scheduledPacketization;
};

struct MediaRealtimeMuxedOutputPlan {
    std::string url;
    std::string format;
    std::string mediaId;
    std::optional<MediaOutputResourceKind> outputResourceKind;
    std::optional<MediaMuxSessionKind> muxSessionKind;
    std::optional<MediaRtpUdpSenderConfig> rtpTransport;
    std::string sdpPath;
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

struct MediaRealtimeOutputPlanningDraft final {
    bool packetCopyNormalizationRequired = false;
    MediaRealtimeRtpOutputNodePlan videoOutput;
    MediaRealtimeRtpOutputNodePlan audioOutput;
    MediaRealtimeMuxedOutputPlan muxedOutput;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan singleStreamMux;
};

struct MediaRealtimeSingleStreamOutputPlan final {
    bool packetCopyNormalizationRequired = false;
    MediaRealtimeRtpOutputNodePlan rtpOutput;
    MediaRealtimeMuxedOutputPlan muxedOutput;
    MediaRealtimeSdpWriterPlan sdp;
    MediaRealtimeMuxNodePlan mux;
};

struct MediaRealtimeRtpTranscodePlan {
    RealtimeInputType inputType;
    RealtimeInputStreamLayout inputLayout;
    RealtimeOutputStreamLayout outputLayout;
    MediaOutputTransportKind outputTransport;
    MediaPipelinePlan videoPlan;
    MediaAudioPipelinePlan audioPlan;
    MediaVideoTranscodeParameters videoParameters;
    MediaGraphQueueParameters queues;
    MediaRealtimeEdgePolicySet edgePolicies;
    MediaThreadingPolicy threadingPolicy;
    bool videoInputStartRequiresKeyFrame = false;
    MediaRealtimeRtpInputNodePlan input;
    MediaRealtimeRtpInputNodePlan audioInput;
    bool useIsolatedAudioInput = false;
    std::optional<MediaRealtimeSingleStreamOutputPlan> singleStreamOutput;
    std::optional<MediaRealtimeAvSyncComponentBounds> avSyncComponentBounds;
    std::optional<MediaRealtimeAvSyncRuntimePlan> avSyncRuntime;
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
    static ::media::Result<MediaRealtimeRtpTranscodePlan> planPreparedInput(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputStreamInfo& input,
        const MediaTsSelectedProgramPlan& selectedTsProgram);
    static ::media::Status validateRealtimeRequestNoIo(
        const MediaRealtimeRtpTranscodeRequest& request);
    static ::media::Status validatePlannedProduct(
        const MediaRealtimeRtpTranscodePlan& plan);

private:
    static ::media::Result<MediaRealtimeRtpTranscodePlan> planWithInput(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimeInputStreamInfo* inputInfo,
        const MediaTsSelectedProgramPlan* selectedTsProgram,
        const MediaPreparedRealtimeInput* preparedInput);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflightImpl(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimePreflightIo* io);
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
