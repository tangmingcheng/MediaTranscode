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

::media::Status validateUserVideoOptions(const MediaVideoTranscodeParameters& video)
{
    if (video.width && *video.width <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier rejects zero/negative width; omit width to keep source size"));
    }

    if (video.height && *video.height <= 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier rejects zero/negative height; omit height to keep source size"));
    }

    if (video.width.has_value() != video.height.has_value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires width and height to be specified together"));
    }

    if (!video.frameRate.complete()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires fps numerator and denominator to be specified together"));
    }

    auto status = validateOptionalPositive(video.frameRate.numerator, "fps numerator");
    if (!status) {
        return status;
    }

    status = validateOptionalPositive(video.frameRate.denominator, "fps denominator");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(video.bitrateKbps, "video bitrate");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(video.minBitrateKbps, "video min bitrate");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(video.maxBitrateKbps, "video max bitrate");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(video.bufferSizeKbits, "video buffer size");
    if (!status) {
        return status;
    }

    if (video.minBitrateKbps && video.maxBitrateKbps && *video.minBitrateKbps > *video.maxBitrateKbps) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("LocalFileNodeOptionApplier requires video min bitrate <= max bitrate"));
    }

    status = validateOptionalNonNegative(video.quality, "quality");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(video.gop, "gop");
    if (!status) {
        return status;
    }

    status = validateOptionalNonNegative(video.bFrames, "bframes");
    if (!status) {
        return status;
    }

    return ::media::Status::success();
}

void applyUserVideoOptionsToNode(MediaGraph& graph, MediaNodeId nodeId, const MediaVideoTranscodeParameters& video)
{
    setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoCodec, video.codecName);
    setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoEncoder, video.encoderName);
    graph.setNodeOption(nodeId, MediaTranscodeOptionKey::VideoRateControl, mediaRateControlModeName(video.rateControl));

    setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoPreset, video.preset);
    setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoProfile, video.profile);
    setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoTune, video.tune);
    setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoLevel, video.level);

    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoWidth, video.width);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoHeight, video.height);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoFpsNum, video.frameRate.numerator);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoFpsDen, video.frameRate.denominator);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBitrateKbps, video.bitrateKbps);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoMinBitrateKbps, video.minBitrateKbps);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoMaxBitrateKbps, video.maxBitrateKbps);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBufferSizeKbits, video.bufferSizeKbits);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoQuality, video.quality);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoGop, video.gop);
    setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBFrames, video.bFrames);
}

} // namespace

::media::Status LocalFileNodeOptionApplier::applyUserVideoOptions(MediaGraph& graph,
                                                                  const LocalFilePlannerNodeIds& nodes,
                                                                  const LocalFileTranscodeOptions& options)
{
    const MediaVideoTranscodeParameters& video = options.parameters.video;
    auto validation = validateUserVideoOptions(video);
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
        applyUserVideoOptionsToNode(graph, nodeId, video);
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
