#pragma once
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
    std::optional<MediaVideoColorRangeFact> effectiveColorRange;
    std::optional<MediaEncoderRateControlPlan> encoderRateControl;
    std::optional<MediaEncoderOpenContract> encoderOpenContract;
    std::optional<MediaPreparedEncoderEmissionEnvelope> preparedEmission;
    std::optional<MediaDecoderInputRetention> preparedInputRetention;
    std::optional<MediaPreparedVideoRandomAccessEnvelope> randomAccess;

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

} // namespace media::ffmpeg::graph
