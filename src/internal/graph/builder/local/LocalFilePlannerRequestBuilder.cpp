#include "internal/graph/builder/local/LocalFilePlannerRequestBuilder.h"

namespace media::ffmpeg::graph {
namespace {

::media::Status validateResizeOptions(const LocalFileTranscodeOptions& options)
{
    if (options.width < 0 || options.height < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires non-negative dimensions"));
    }

    const bool widthSpecified = options.width > 0;
    const bool heightSpecified = options.height > 0;
    if (widthSpecified != heightSpecified) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFilePlannerRequestBuilder requires width and height to be specified together"));
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
    plannerOptions.targetWidth = options.width;
    plannerOptions.targetHeight = options.height;
    plannerOptions.allowSoftwareFallback = false;
    plannerOptions.requireRuntimeAvailability = true;
    plannerOptions.preferGpu = options.useHardwareTransfer && !options.disableHardware;
    plannerOptions.preferredHardware = plannerOptions.preferGpu ? "auto" : "software";
    plannerOptions.diagnosticLogEnabled = options.diagnosticLogEnabled;
    return ::media::Result<MediaPipelinePlannerOptions>::success(std::move(plannerOptions));
}

} // namespace media::ffmpeg::graph
