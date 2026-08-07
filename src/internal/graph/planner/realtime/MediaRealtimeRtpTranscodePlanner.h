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
#include "internal/graph/protocol/mpegts/MediaTsProgramSelection.h"
#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"
#include "internal/graph/protocol/rtp/MediaRtpClockObservationSchedule.h"
#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/protocol/mpegts/MediaTsPacketOriginPolicy.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>
#include <variant>

namespace media::ffmpeg::graph {

struct MediaRealtimeInputStreamInfo;
struct MediaDetectedRtpVideoSignaling;
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
    MediaRtpClockLossPolicy clockLossPolicy;
    std::optional<MediaRtcpCompositionMode> rtcpCompositionMode;
};

struct MediaRealtimeTsInputPolicy final {
    MediaRealtimeTsInputPolicy() = delete;

    std::string demuxFormat;
    std::size_t packetSize;
    std::size_t avioBufferBytes;
    std::size_t maximumDatagramBytes;
    std::size_t evidenceTimelineCapacity;
    std::uint64_t maximumPacketPositionRegressionBytes;
    std::size_t pesProvenanceCapacity;
    MediaTsPacketOriginPolicy packetOriginPolicy;

    static ::media::Result<MediaRealtimeTsInputPolicy> create(
        std::size_t packetSize,
        std::uint64_t probeWindowBytes,
        std::uint64_t maximumPacketPositionRegressionBytes,
        std::size_t evidenceTimelineCapacity,
        std::size_t selectedStreamCount);
    static ::media::Result<std::size_t> minimumEvidenceCapacity(
        std::size_t packetSize,
        std::uint64_t probeWindowBytes,
        std::uint64_t maximumPacketPositionRegressionBytes);

private:
    MediaRealtimeTsInputPolicy(
        std::string selectedDemuxFormat,
        std::size_t selectedPacketSize,
        std::size_t selectedAvioBufferBytes,
        std::size_t selectedMaximumDatagramBytes,
        std::size_t selectedEvidenceTimelineCapacity,
        std::uint64_t selectedMaximumPacketPositionRegressionBytes,
        std::size_t selectedPesProvenanceCapacity,
        MediaTsPacketOriginPolicy selectedPacketOriginPolicy) noexcept;
};

struct MediaRealtimeTsInputPlan final {
    struct VideoOnlyRetention final {
        VideoOnlyRetention() = delete;
        VideoOnlyRetention(std::size_t packetCapacity,
                           std::uint64_t byteCapacity,
                           std::uint64_t maximumPacketBytes) noexcept
            : videoPacketCapacity(packetCapacity),
              videoByteCapacity(byteCapacity),
              maximumVideoPacketBytes(maximumPacketBytes)
        {
        }
        std::size_t videoPacketCapacity;
        std::uint64_t videoByteCapacity;
        std::uint64_t maximumVideoPacketBytes;
        bool operator==(const VideoOnlyRetention&) const = default;
    };
    struct AudioVideoRetention final {
        AudioVideoRetention() = delete;
        AudioVideoRetention(
            std::size_t selectedVideoPacketCapacity,
            std::size_t selectedAudioPacketCapacity,
            std::uint64_t selectedVideoByteCapacity,
            std::uint64_t selectedAudioByteCapacity,
            std::uint64_t selectedMaximumVideoPacketBytes,
            std::uint64_t selectedMaximumAudioPacketBytes) noexcept
            : videoPacketCapacity(selectedVideoPacketCapacity),
              audioPacketCapacity(selectedAudioPacketCapacity),
              videoByteCapacity(selectedVideoByteCapacity),
              audioByteCapacity(selectedAudioByteCapacity),
              maximumVideoPacketBytes(selectedMaximumVideoPacketBytes),
              maximumAudioPacketBytes(selectedMaximumAudioPacketBytes)
        {
        }
        std::size_t videoPacketCapacity;
        std::size_t audioPacketCapacity;
        std::uint64_t videoByteCapacity;
        std::uint64_t audioByteCapacity;
        std::uint64_t maximumVideoPacketBytes;
        std::uint64_t maximumAudioPacketBytes;
        bool operator==(const AudioVideoRetention&) const = default;
    };
    using Retention = std::variant<VideoOnlyRetention, AudioVideoRetention>;

    std::string demuxFormat;
    std::size_t packetSize;
    std::size_t avioBufferBytes;
    std::size_t maximumDatagramBytes;
    std::size_t evidenceTimelineCapacity;
    std::uint64_t maximumPacketPositionRegressionBytes;
    std::size_t pesProvenanceCapacity;
    MediaTsPacketOriginPolicy packetOriginPolicy;
    MediaTsSelectedProgramPlan selectedProgram;
    std::int64_t maximumPcrGap27Mhz;
    std::size_t projectionCapacity;
    Retention retention;
    std::uint64_t initialSourceGeneration;
    std::uint64_t initialRawTransportGeneration;

    MediaRealtimeTsInputPlan() = delete;
    static ::media::Result<MediaRealtimeTsInputPlan> create(
        MediaRealtimeTsInputPolicy policy,
        MediaTsSelectedProgramPlan selectedProgram,
        std::int64_t maximumPcrGap27Mhz,
        Retention retention,
        std::uint64_t initialSourceGeneration,
        std::uint64_t initialRawTransportGeneration);
    ::media::Status validateProduct() const;

private:
    MediaRealtimeTsInputPlan(
        MediaRealtimeTsInputPolicy policy,
        MediaTsSelectedProgramPlan selectedProgram,
        std::int64_t maximumPcrGap27Mhz,
        Retention retention,
        std::uint64_t initialSourceGeneration,
        std::uint64_t initialRawTransportGeneration) noexcept;
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
    std::optional<bool> requiresPreparedInput;
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
    std::optional<MediaPreparedRealtimeInputKind> requiredPreparedInputKind;
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
    std::optional<MediaPreparedRealtimeInput> preparedAudio;
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
        const MediaPreparedRealtimeInput* preparedInput,
        const MediaPreparedRealtimeInput* preparedAudioInput,
        std::optional<MediaPipelinePlan> preplannedVideo = std::nullopt,
        const MediaDetectedRtpVideoSignaling* detectedVideoSignaling = nullptr);
    static ::media::Result<MediaRealtimeTranscodePreflight> preflightImpl(
        const MediaRealtimeRtpTranscodeRequest& request,
        const MediaRealtimePreflightIo* io);
    MediaRealtimeRtpTranscodePlanner() = default;
};

} // namespace media::ffmpeg::graph
