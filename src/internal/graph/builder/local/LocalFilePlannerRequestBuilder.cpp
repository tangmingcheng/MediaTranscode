#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Status validateResizeOptions(const LocalFileTranscodeOptions& options)
{
    if (options.width && *options.width < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires non-negative width"));
    }

    if (options.height && *options.height < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires non-negative height"));
    }

    if (options.width.has_value() != options.height.has_value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires width and height to be specified together"));
    }

    if ((options.width && *options.width == 0) || (options.height && *options.height == 0)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder rejects zero resize dimensions; omit width and height to keep source size"));
    }

    return ::media::Status::success();
}

} // namespace

::media::Result<MediaPipelinePlannerOptions> LocalFilePlannerRequestBuilder::buildVideoPlannerOptions(
    const LocalFileTranscodeOptions& options)
{
    auto resizeValidation = validateResizeOptions(options);
    if (!resizeValidation) {
        return ::media::Result<MediaPipelinePlannerOptions>::failure(resizeValidation.error());
    }

    MediaPipelinePlannerOptions plannerOptions;
    plannerOptions.outputPath = options.outputUrl;
    plannerOptions.outputCodecName = options.videoCodec;
    plannerOptions.targetWidth = options.width.value_or(0);
    plannerOptions.targetHeight = options.height.value_or(0);
    plannerOptions.allowSoftwareFallback = false;
    plannerOptions.requireRuntimeAvailability = true;
    plannerOptions.preferGpu = options.useHardwareTransfer && !options.disableHardware;
    plannerOptions.preferredHardware = plannerOptions.preferGpu ? "auto" : "software";
    plannerOptions.diagnosticLogEnabled = options.diagnosticLogEnabled;
    return ::media::Result<MediaPipelinePlannerOptions>::success(std::move(plannerOptions));
}

} // namespace media::ffmpeg::graph
