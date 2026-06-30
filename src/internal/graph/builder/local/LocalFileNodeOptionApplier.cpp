#include "internal/graph/builder/local/LocalFileNodeOptionApplier.h"

#include <array>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {
namespace {

void setIfNotEmpty(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    if (!value.empty()) {
        graph.setNodeOption(nodeId, key, value);
    }
}

void setIfPresent(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::optional<int>& value)
{
    if (value) {
        graph.setNodeOption(nodeId, key, std::to_string(*value));
    }
}

::media::Status validateOptionalNonNegative(const std::optional<int>& value, const std::string& name)
{
    if (value && *value < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires non-negative " + name));
    }

    return ::media::Status::success();
}

::media::Status validateOptionalPositive(const std::optional<int>& value, const std::string& name)
{
    if (value && *value <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires positive " + name));
    }

    return ::media::Status::success();
}

::media::Status validateUserVideoOptions(const LocalFileTranscodeOptions& options)
{
    if (options.width && *options.width <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier rejects zero/negative width; omit width to keep source size"));
    }

    if (options.height && *options.height <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier rejects zero/negative height; omit height to keep source size"));
    }

    if (options.width.has_value() != options.height.has_value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires width and height to be specified together"));
    }

    if (options.fpsNum.has_value() != options.fpsDen.has_value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires fps numerator and denominator to be specified together"));
    }

    auto status = validateOptionalPositive(options.fpsNum, "fps numerator");
    if (!status) {
        return status;
    }

    status = validateOptionalPositive(options.fpsDen, "fps denominator");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(options.videoBitrateKbps, "video bitrate");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(options.crf, "crf");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(options.quality, "quality");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(options.gop, "gop");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(options.maxBFrames, "bframes");
    if (!status) {
        return status;
    }

    return ::media::Status::success();
}

void applyUserVideoOptionsToNode(MediaGraph& graph, MediaNodeId nodeId, const LocalFileTranscodeOptions& options)
{
    setIfNotEmpty(graph, nodeId, "video_codec", options.videoCodec);
    setIfNotEmpty(graph, nodeId, "encoder", options.videoEncoder);

    if (!options.rateControlMode.empty() && options.rateControlMode != "auto") {
        graph.setNodeOption(nodeId, "rc", options.rateControlMode);
    }

    setIfNotEmpty(graph, nodeId, "preset", options.speedPreset);
    setIfNotEmpty(graph, nodeId, "profile", options.profile);
    setIfNotEmpty(graph, nodeId, "tune", options.tune);
    setIfNotEmpty(graph, nodeId, "level", options.level);

    setIfPresent(graph, nodeId, "width", options.width);
    setIfPresent(graph, nodeId, "height", options.height);
    setIfPresent(graph, nodeId, "fps_num", options.fpsNum);
    setIfPresent(graph, nodeId, "fps_den", options.fpsDen);
    setIfPresent(graph, nodeId, "bitrate_kbps", options.videoBitrateKbps);
    setIfPresent(graph, nodeId, "crf", options.crf);
    setIfPresent(graph, nodeId, "quality", options.quality);
    setIfPresent(graph, nodeId, "gop", options.gop);
    setIfPresent(graph, nodeId, "bframes", options.maxBFrames);
}

} // namespace

::media::Status LocalFileNodeOptionApplier::applyUserVideoOptions(MediaGraph& graph,
                                                                  const LocalFilePlannerNodeIds& nodes,
                                                                  const LocalFileTranscodeOptions& options)
{
    auto validation = validateUserVideoOptions(options);
    if (!validation) {
        return validation;
    }

    const std::array<MediaNodeId, 6> videoOptionNodes {
        nodes.codecResolver,
        nodes.hardwareTransfer,
        nodes.videoTimestamp,
        nodes.videoFrameRate,
        nodes.videoFilter,
        nodes.videoEncode,
    };

    for (MediaNodeId nodeId : videoOptionNodes) {
        applyUserVideoOptionsToNode(graph, nodeId, options);
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
