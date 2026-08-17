#include "internal/graph/builder/segments/MediaVideoTranscodeOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

#include <array>
#include <optional>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaVideoTranscodeOptionApplier";

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setIfNotEmpty(MediaGraph& graph,
                                    MediaNodeId nodeId,
                                    const std::string& key,
                                    const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, value);
}

::media::Result<void> setIfPresent(MediaGraph& graph,
                                   MediaNodeId nodeId,
                                   const std::string& key,
                                   const std::optional<int>& value)
{
    if (!value) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, std::to_string(*value));
}

::media::Result<void> setIfPresent(MediaGraph& graph,
                                   MediaNodeId nodeId,
                                   const std::string& key,
                                   const std::optional<bool>& value)
{
    if (!value) {
        return ::media::Result<void>::success();
    }
    return setOption(graph, nodeId, key, *value ? "1" : "0");
}

::media::Result<void> validateOptionalNonNegative(const std::optional<int>& value,
                                                  const std::string& name)
{
    if (value && *value < 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier requires non-negative " + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateOptionalPositive(const std::optional<int>& value,
                                               const std::string& name)
{
    if (value && *value <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier requires positive " + name));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> validateUserVideoOptions(const MediaVideoTranscodeParameters& video)
{
    if (video.width && *video.width <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier rejects zero/negative width; omit width to keep source size"));
    }
    if (video.height && *video.height <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier rejects zero/negative height; omit height to keep source size"));
    }
    if (video.width.has_value() != video.height.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier requires width and height to be specified together"));
    }
    if (!video.frameRate.complete()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier requires fps numerator and denominator to be specified together"));
    }

    if (auto status = validateOptionalPositive(video.frameRate.numerator, "fps numerator"); !status) return status;
    if (auto status = validateOptionalPositive(video.frameRate.denominator, "fps denominator"); !status) return status;
    if (auto status = validateOptionalPositive(video.bitrateKbps, "video bitrate"); !status) return status;
    if (auto status = validateOptionalPositive(video.minBitrateKbps, "video min bitrate"); !status) return status;
    if (auto status = validateOptionalPositive(video.maxBitrateKbps, "video max bitrate"); !status) return status;
    if (auto status = validateOptionalPositive(video.bufferSizeKbits, "video buffer size"); !status) return status;
    if (video.minBitrateKbps && video.maxBitrateKbps && *video.minBitrateKbps > *video.maxBitrateKbps) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaVideoTranscodeOptionApplier requires video min bitrate <= max bitrate"));
    }
    if (auto status = validateOptionalNonNegative(video.quality, "quality"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.gop, "gop"); !status) return status;
    if (auto status = validateOptionalNonNegative(video.bFrames, "bframes"); !status) return status;
    return ::media::Result<void>::success();
}

::media::Result<void> applyUserVideoOptionsToNode(MediaGraph& graph,
                                                  MediaNodeId nodeId,
                                                  const MediaVideoTranscodeParameters& video)
{
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoCodec, video.codecName); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoPreset, video.preset); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoProfile, video.profile); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoTune, video.tune); !status) return status;
    if (auto status = setIfNotEmpty(graph, nodeId, MediaTranscodeOptionKey::VideoLevel, video.level); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoWidth, video.width); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoHeight, video.height); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoFpsNum, video.frameRate.numerator); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoFpsDen, video.frameRate.denominator); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoQuality, video.quality); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoGop, video.gop); !status) return status;
    if (auto status = setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoBFrames, video.bFrames); !status) return status;
    return setIfPresent(graph, nodeId, MediaTranscodeOptionKey::VideoGlobalHeader, video.globalHeader);
}

} // namespace

::media::Result<void> MediaVideoTranscodeOptionApplier::applyUserOptions(
    MediaGraph& graph,
    const MediaVideoTranscodeBranchNodes& nodes,
    const MediaVideoTranscodeParameters& video)
{
    if (auto status = validateUserVideoOptions(video); !status) return status;

    const std::array<MediaNodeId, 6> videoOptionNodes {
        nodes.codecResolver,
        nodes.hardwareTransfer,
        nodes.videoTimestamp,
        nodes.videoFrameRate,
        nodes.videoFilter,
        nodes.videoEncode,
    };

    for (MediaNodeId nodeId : videoOptionNodes) {
        if (!nodeId.isValid()) continue;
        if (auto status = applyUserVideoOptionsToNode(graph, nodeId, video); !status) return status;
    }
    return ::media::Result<void>::success();
}

} // namespace media::ffmpeg::graph
