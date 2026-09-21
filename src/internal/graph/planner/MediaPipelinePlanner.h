#pragma once
#include "internal/graph/planner/video/MediaVideoSourcePlan.h"
#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include "internal/graph/model/MediaPreparedVideoRandomAccessEnvelope.h"
#include "internal/graph/model/MediaVideoSharedSourcePlan.h"

#include "internal/graph/model/MediaVideoEncodingRequestContract.h"

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaDecoderInputRetention.h"
#include "internal/graph/model/MediaVideoSourceEpochPlan.h"
#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/model/MediaEncoderOpenContract.h"
#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"
#include "internal/graph/time/MediaRunningTime.h"
#include "internal/graph/planner/MediaPreparedEncoderEmissionEnvelope.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaHardwareCapabilityProbe;

struct MediaPipelineChainPlan : MediaVideoSourcePlan {
    MediaPipelineStagePlan encoder;
    MediaVideoLineagePropagation encoderLineagePropagation =
        MediaVideoLineagePropagation::Unknown;
    MediaVideoEncoderAbortPolicy encoderAbortPolicy =
        MediaVideoEncoderAbortPolicy::Unknown;
};

struct MediaPipelinePlannerOptions {
    MediaPipelinePlannerOptions() = delete;

    MediaPipelinePlannerOptions(bool allowPacketCopy,
                                bool filterRequired,
                                bool lowLatency) noexcept
        : allowPacketCopy(allowPacketCopy),
          filterRequired(filterRequired),
          lowLatency(lowLatency)
    {
    }

    bool allowPacketCopy;
    std::string outputPath;
    std::string outputCodecName;
    int probeWidth = 0;
    int probeHeight = 0;
    MediaRational sourceFrameRate;
    std::optional<AVColorRange> sourceColorRange;
    MediaRational targetFrameRate;
    MediaEncoderRateControlRequest encoderRateControl;
    MediaVideoTranscodeParameters encoderOpenRequest;
    int targetWidth = 0;
    int targetHeight = 0;
    bool filterRequired;
    bool diagnosticLogEnabled = false;
    std::string rtspTransport;
    int openTimeoutMs = 0;
    int readTimeoutMs = 0;
    int analyzeDurationUs = 0;
    int probeSizeBytes = 0;
    bool lowLatency;
};

struct MediaInputVideoStreamInfo {
    int streamIndex = invalidMediaStreamIndex;
    std::string codecName;
    int width = 0;
    int height = 0;
    int64_t bitrateBitsPerSecond = 0;
    std::optional<AVColorRange> sourceColorRange;
    MediaRational frameRate;
    MediaRational sampleAspectRatio;
};

enum class MediaVideoNoOutputPolicy { Consume };
enum class MediaVideoOutputOverflowPolicy { FailBranch };

struct MediaVideoOutputFanoutPlan final {
    MediaVideoNoOutputPolicy noOutputs;
    MediaVideoOutputOverflowPolicy overflow;
};

struct MediaPipelinePlan {
    std::optional<MediaVideoSharedSourcePlan> sharedSource;
    std::optional<MediaVideoOutputFanoutPlan> outputFanout;
    std::optional<MediaVideoSourceEpochPlan> sourcePlaybackEpoch;
    std::optional<MediaVideoOutputFanoutPlan> encodedOutputFanout;
    bool enabled = false;
    MediaBranchMode branchMode = MediaBranchMode::Drop;
    int sourceStreamIndex = invalidMediaStreamIndex;
    std::string reason;
    std::string inputPath;
    std::string outputPath;
    std::string inputCodecName;
    std::string outputCodecName;
    bool diagnosticLogEnabled = false;
    bool synthesizeMissingTimestamps = false;
    bool filterActive = false;
    MediaPipelineChainPlan selected;
    std::optional<MediaRational> maximumFrameDuplicationGap;
    std::vector<MediaPipelineChainPlan> candidates;
    std::optional<MediaVideoEncodingRequestContract> encodingRequest;
};

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept;
const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept;
const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept;

class MediaPipelinePlanner final {
public:
    static ::media::Result<MediaVideoEncodingRequestContract> normalizeEncodingRequest(
        const MediaInputVideoStreamInfo& source,
        const MediaPipelinePlannerOptions& options,
        const MediaPipelineStagePlan& selectedEncoder);

    static ::media::Result<MediaPipelinePlan> planVideoTranscodeFile(
        const std::string& inputPath,
        MediaPipelinePlannerOptions options);

    static ::media::Result<MediaPipelinePlan> planVideoTranscodeRealtimeUrl(
        const std::string& inputUrl,
        MediaPipelinePlannerOptions options);

    static ::media::Result<MediaPipelinePlan> planVideoTranscodeKnownInput(
        MediaInputVideoStreamInfo inputInfo,
        const std::string& inputUrl,
        MediaPipelinePlannerOptions options);

    static ::media::Status materializeSourceExecutionContract(
        MediaVideoSourcePlan& source, const MediaRational& sourceFrameRate);
    static ::media::Result<std::vector<MediaVideoSourcePlan>> planVideoSourceCandidates(
        const MediaInputVideoStreamInfo& input, const MediaVideoSourcePlanningOptions& options,
        const MediaHardwareDescriptor& target);

    static ::media::Status preflightSelectedCandidate(
        MediaPipelineChainPlan& selected,
        const MediaPipelinePlannerOptions& options,
        MediaHardwareCapabilityProbe& hardwareProbe);

    static ::media::Result<MediaPipelinePlan> planVideoOutputBranch(
        MediaInputVideoStreamInfo inputInfo,
        const std::string& inputUrl,
        const MediaPipelineStagePlan& runningDecoder,
        MediaPipelinePlannerOptions options,
        MediaHardwareCapabilityProbe& outputProbe);

private:
    MediaPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
