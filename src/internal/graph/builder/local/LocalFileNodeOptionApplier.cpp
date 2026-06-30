#include "internal/graph/builder/local/LocalFileNodeOptionApplier.h"

#include <array>
#include <string>

namespace media::ffmpeg::graph {
namespace {

void setIfNotEmpty(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, const std::string& value)
{
    if (!value.empty()) {
        graph.setNodeOption(nodeId, key, value);
    }
}

void setIfPositive(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, int value)
{
    if (value > 0) {
        graph.setNodeOption(nodeId, key, std::to_string(value));
    }
}

void setIfNonNegative(MediaGraph& graph, MediaNodeId nodeId, const std::string& key, int value)
{
    if (value >= 0) {
        graph.setNodeOption(nodeId, key, std::to_string(value));
    }
}

::media::Status validateUserVideoOptions(const LocalFileTranscodeOptions& options)
{
    if (options.width < 0 || options.height < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires non-negative dimensions"));
    }

    const bool widthSpecified = options.width > 0;
    const bool heightSpecified = options.height > 0;
    if (widthSpecified != heightSpecified) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires width and height to be specified together"));
    }

    if (options.fpsNum < 0 || options.fpsDen <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires a positive fps denominator"));
    }

    if (options.videoBitrateKbps < 0 || options.crf < -1 || options.quality < -1 ||
        options.gop < 0 || options.maxBFrames < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier received invalid negative video option"));
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

    if (options.width > 0 && options.height > 0) {
        graph.setNodeOption(nodeId, "width", std::to_string(options.width));
        graph.setNodeOption(nodeId, "height", std::to_string(options.height));
    }

    if (options.fpsNum > 0) {
        graph.setNodeOption(nodeId, "fps_num", std::to_string(options.fpsNum));
        graph.setNodeOption(nodeId, "fps_den", std::to_string(options.fpsDen));
    }

    setIfPositive(graph, nodeId, "bitrate_kbps", options.videoBitrateKbps);
    setIfNonNegative(graph, nodeId, "crf", options.crf);
    setIfNonNegative(graph, nodeId, "quality", options.quality);
    setIfPositive(graph, nodeId, "gop", options.gop);
    setIfPositive(graph, nodeId, "bframes", options.maxBFrames);
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
