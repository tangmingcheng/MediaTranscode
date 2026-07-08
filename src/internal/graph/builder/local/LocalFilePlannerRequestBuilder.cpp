#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status validateResizeOptions(const MediaVideoTranscodeParameters& video)
{
    if (video.width && *video.width < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires non-negative width"));
    }

    if (video.height && *video.height < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires non-negative height"));
    }

    if (video.width.has_value() != video.height.has_value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires width and height to be specified together"));
    }

    if ((video.width && *video.width == 0) || (video.height && *video.height == 0)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder rejects zero resize dimensions; omit width and height to keep source size"));
    }

    return ::media::Status::success();
}

bool encodeOptionsRequested(const MediaVideoTranscodeParameters& video) noexcept
{
    return video.frameRate.specified() ||
        video.rateControl != MediaRateControlMode::Auto ||
        video.bitrateKbps.has_value() ||
        video.minBitrateKbps.has_value() ||
        video.maxBitrateKbps.has_value() ||
        video.bufferSizeKbits.has_value() ||
        video.quality.has_value() ||
        !video.preset.empty() ||
        !video.tune.empty() ||
        !video.profile.empty() ||
        !video.level.empty() ||
        video.gop.has_value() ||
        video.bFrames.has_value();
}

} // namespace

::media::Result<MediaPipelinePlannerOptions> LocalFilePlannerRequestBuilder::buildVideoPlannerOptions(
    const LocalFileTranscodeOptions& options)
{
    const MediaTranscodeParameterSet& parameters = options.parameters;
    const MediaVideoTranscodeParameters& video = parameters.video;

    auto resizeValidation = validateResizeOptions(video);
    if (!resizeValidation) {
        return ::media::Result<MediaPipelinePlannerOptions>::failure(resizeValidation.error());
    }

    MediaPipelinePlannerOptions plannerOptions(!video.resizeRequested() && !encodeOptionsRequested(video),
                                               video.resizeRequested(),
                                               !parameters.execution.disableHardware,
                                               parameters.execution.disableHardware,
                                               true,
                                               false);
    plannerOptions.outputPath = options.outputUrl;
    plannerOptions.outputCodecName = video.codecName;
    plannerOptions.targetWidth = video.width.value_or(0);
    plannerOptions.targetHeight = video.height.value_or(0);
    plannerOptions.preferredHardware = plannerOptions.preferGpu ? "auto" : "software";
    plannerOptions.diagnosticLogEnabled = parameters.execution.diagnosticLogEnabled;
    return ::media::Result<MediaPipelinePlannerOptions>::success(std::move(plannerOptions));
}

} // namespace media::ffmpeg::graph
