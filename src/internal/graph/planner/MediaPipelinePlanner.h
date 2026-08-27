#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "internal/graph/model/MediaEncodedPacketLayout.h"
#include "internal/graph/model/MediaEncoderOpenContract.h"
#include "internal/graph/model/MediaEncoderRateControlPlan.h"
#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"
#include "internal/graph/planner/MediaPreparedEncoderEmissionEnvelope.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

class MediaHardwareCapabilityProbe;

enum class MediaPipelineStageRole {
    Decoder,
    Filter,
    Encoder
};

struct MediaPipelineStagePlan {
    MediaPipelineStageRole role = MediaPipelineStageRole::Decoder;
    std::string componentName;
    std::string codecName;
    std::string ffmpegName;
    std::string filterName;
    std::string hwaccelName;
    std::optional<MediaHardwareDescriptor> inputFrame;
    std::optional<MediaHardwareDescriptor> outputFrame;
    bool available = false;
    int priority = 0;
    std::string availabilityReason;
    std::optional<MediaEncodedPacketLayout> encodedPacketLayout;
    std::optional<MediaEncoderRateControlPlan> encoderRateControl;
    std::optional<MediaEncoderOpenContract> encoderOpenContract;
    std::optional<MediaPreparedEncoderEmissionEnvelope> preparedEmission;

    const MediaHardwareDescriptor* frameContract() const noexcept
    {
        return inputFrame ? &*inputFrame : outputFrame ? &*outputFrame : nullptr;
    }
    MediaHardwareDeviceKind deviceKind() const noexcept
    {
        const auto* contract = frameContract();
        return contract ? contract->deviceKind : MediaHardwareDeviceKind::Unknown;
    }
    bool hardware() const noexcept
    {
        const auto* contract = frameContract();
        return contract && contract->isHardwareBacked();
    }
    bool zeroCopy() const noexcept
    {
        const auto* contract = frameContract();
        return contract && contract->zeroCopyPreferred;
    }
};

struct MediaPipelineChainPlan {
    std::string label;
    MediaPipelineStagePlan decoder;
    MediaPipelineStagePlan filter;
    MediaPipelineStagePlan encoder;
    int score = 0;
    bool available = false;
    bool allHardware = false;
    bool sameHardwareDevice = false;
    bool zeroCopy = false;
    bool filterActive = false;
    MediaHardwareTransferDirection transferDirection = MediaHardwareTransferDirection::Unknown;
    MediaVideoLineagePropagation decoderLineagePropagation =
        MediaVideoLineagePropagation::Unknown;
    MediaVideoLineagePropagation encoderLineagePropagation =
        MediaVideoLineagePropagation::Unknown;
    MediaVideoFilterImplementation filterImplementation =
        MediaVideoFilterImplementation::Unknown;
    MediaVideoEncoderAbortPolicy encoderAbortPolicy =
        MediaVideoEncoderAbortPolicy::Unknown;
    std::string reason;
};

struct MediaPipelinePlannerOptions {
    MediaPipelinePlannerOptions() = delete;

    MediaPipelinePlannerOptions(bool allowPacketCopy,
                                bool filterRequired,
                                bool disableHardware,
                                bool lowLatency) noexcept
        : allowPacketCopy(allowPacketCopy),
          filterRequired(filterRequired),
          disableHardware(disableHardware),
          lowLatency(lowLatency)
    {
    }

    bool allowPacketCopy;
    std::string outputPath;
    std::string outputCodecName;
    int probeWidth = 0;
    int probeHeight = 0;
    MediaRational sourceFrameRate;
    MediaRational targetFrameRate;
    MediaEncoderRateControlRequest encoderRateControl;
    struct EncoderVbvPlan final {
        int bufferSizeKbits = 0;
        std::string authority;
    };
    std::optional<EncoderVbvPlan> encoderVbvPlan;
    MediaVideoTranscodeParameters encoderOpenRequest;
    int targetWidth = 0;
    int targetHeight = 0;
    bool filterRequired;
    MediaHardwareBackendRequest hardwareBackend = MediaHardwareBackendRequest::Auto;
    bool disableHardware;
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
    MediaRational frameRate;
};

struct MediaPipelinePlan {
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
    std::vector<MediaPipelineChainPlan> candidates;
};

const char* mediaPipelineStageRoleName(MediaPipelineStageRole role) noexcept;
const char* mediaHardwareDeviceKindName(MediaHardwareDeviceKind kind) noexcept;
const char* mediaHardwareFrameKindName(MediaHardwareFrameKind kind) noexcept;

class MediaPipelinePlanner final {
public:
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

    static ::media::Result<std::size_t> selectHighestRankedCandidate(
        const std::vector<MediaPipelineChainPlan>& candidates,
        const MediaPipelinePlannerOptions& options);

    static ::media::Status preflightSelectedCandidate(
        MediaPipelineChainPlan& selected,
        const MediaPipelinePlannerOptions& options,
        MediaHardwareCapabilityProbe& hardwareProbe);

private:
    MediaPipelinePlanner() = default;
};

} // namespace media::ffmpeg::graph
