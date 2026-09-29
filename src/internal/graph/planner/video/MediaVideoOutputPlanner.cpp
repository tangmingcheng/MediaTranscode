#include "internal/graph/planner/video/MediaVideoOutputPlanner.h"

#include "internal/graph/planner/MediaEncoderRateControlPlanner.h"

#include <limits>
#include <utility>

namespace media::ffmpeg::graph {

::media::Result<MediaEncoderOpenContract> MediaVideoOutputPlanner::planOpenContract(
    const MediaPipelineStagePlan& encoder, MediaSize outputSize,
    MediaRational outputFrameRate, const MediaEncoderRateControlRequest& rateControlRequest,
    const MediaVideoTranscodeParameters& request, bool lowLatency)
{
    using Result = ::media::Result<MediaEncoderOpenContract>;
    if (encoder.role != MediaPipelineStageRole::Encoder || encoder.ffmpegName.empty() ||
        outputSize.width <= 0 || outputSize.height <= 0 || !outputFrameRate.isKnown()) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "output encoder planning requires explicit encoder identity, dimensions and cadence"));
    }
    auto rateControl = MediaEncoderRateControlPlanner::plan(encoder.ffmpegName,
        encoder.deviceKind(), rateControlRequest, outputFrameRate, lowLatency);
    if (!rateControl) return Result::failure(rateControl.error());
    return Result::success({encoder.ffmpegName, outputSize.width, outputSize.height,
        outputFrameRate, std::move(rateControl).value(), request.quality, request.preset,
        request.tune, request.profile, request.level, request.gop, request.bFrames,
        request.globalHeader, lowLatency});
}

::media::Status MediaVideoOutputPlanner::materializeExecutionContract(MediaVideoOutputPlan& output)
{
    if (output.encoder.role != MediaPipelineStageRole::Encoder ||
        !output.encoder.inputFrame || output.encoder.deviceKind() == MediaHardwareDeviceKind::Unknown) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "output encoder execution requires its selected frame domain"));
    }
    const bool rkmpp = output.encoder.deviceKind() == MediaHardwareDeviceKind::RKMPP;
    output.encoderLineagePropagation = rkmpp
        ? MediaVideoLineagePropagation::SubmissionOrder : MediaVideoLineagePropagation::CodecCopyOpaque;
    output.encoderAbortPolicy = rkmpp
        ? MediaVideoEncoderAbortPolicy::DrainThenAbort : MediaVideoEncoderAbortPolicy::Immediate;
    return ::media::Status::success();
}

::media::Status MediaVideoOutputPlanner::completePreparedRateControl(MediaVideoOutputPlan& output)
{
    auto& encoder = output.encoder;
    if (!encoder.encoderRateControl || !encoder.encoderOpenContract || !encoder.preparedEmission) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "encoder rate-control completion requires open and prepared emission contracts"));
    }
    auto completed = *encoder.encoderRateControl;
    if (!completed.bufferSizeKbits) {
        constexpr std::uint64_t BitsPerKilobit = 1000;
        const auto effectiveBits = encoder.preparedEmission->effectiveVbvBufferBits;
        if (effectiveBits) {
            if (*effectiveBits == 0 || *effectiveBits % BitsPerKilobit != 0 ||
                *effectiveBits / BitsPerKilobit > static_cast<std::uint64_t>(
                    (std::numeric_limits<int>::max)())) {
                return ::media::Status::failure(::media::ErrorInfo::notInitialized(
                    "opened encoder did not expose an exactly representable effective VBV readback"));
            }
            completed.bufferSizeKbits = static_cast<int>(*effectiveBits / BitsPerKilobit);
        }
    }
    encoder.encoderRateControl = std::move(completed);
    encoder.encoderOpenContract->rateControl = *encoder.encoderRateControl;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
