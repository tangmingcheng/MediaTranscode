#include "internal/graph/planner/realtime/MediaGraphFrameCreditContractPlanner.h"
#include <charconv>
#include <utility>

namespace media::ffmpeg::graph {
::media::Result<std::string> MediaGraphFrameCreditContractPlanner::videoOutputPrefix(const MediaNode& node)
{
    using Result = ::media::Result<std::string>;
    std::string prefix;
    if (node.kind == MediaNodeKind::VideoDecode) {
        prefix = "decoder.pipeline.output";
    } else if (node.kind == MediaNodeKind::VideoFilter) {
        prefix = "filter.pipeline.output";
    } else if (node.kind == MediaNodeKind::HardwareTransfer) {
        if (!node.options.has("transfer.direction")) {
            return Result::failure(::media::ErrorInfo::notInitialized(
                "hardware transfer frame credit lacks planner direction"));
        }
        const auto direction = node.options.value("transfer.direction");
        if (direction == "none") {
            prefix = "decoder.pipeline.output";
        } else if (direction == "download") {
            if (!node.options.has("pipeline.filter_active")) {
                return Result::failure(::media::ErrorInfo::notInitialized(
                    "hardware transfer frame credit lacks filter topology fact"));
            }
            const auto filterActive =
                node.options.value("pipeline.filter_active");
            if (filterActive != "0" && filterActive != "1") {
                return Result::failure(::media::ErrorInfo::invalidArgument(
                    "hardware transfer frame credit has invalid filter topology fact"));
            }
            prefix = filterActive == "1"
                ? "filter.pipeline.input" : "encoder.pipeline.input";
        } else {
            return Result::failure(::media::ErrorInfo::unsupported(
                "hardware transfer frame credit has no admitted output contract"));
        }
    } else {
        return Result::failure(::media::ErrorInfo::unsupported(
            "frame payload producer lacks a typed frame credit resolver"));
    }

    return Result::success(std::move(prefix));
}

::media::Result<MediaFrameCreditContract> MediaGraphFrameCreditContractPlanner::plan(
    const MediaNode& node,
    std::uint64_t maximumLogicalBytes)
{
    using Result = ::media::Result<MediaFrameCreditContract>;
    if (node.kind == MediaNodeKind::AudioDecode ||
        node.kind == MediaNodeKind::AudioStartupTrim ||
        node.kind == MediaNodeKind::AudioResample) {
        return Result::success(MediaFrameCreditContract{
            MediaFrameCreditAllocationScope::EngineLogicalBytes,
            maximumLogicalBytes, 1,
            "prepared-audio-frame-footprint+avframe-software-samples"});
    }
    auto resolvedPrefix = videoOutputPrefix(node);
    if (!resolvedPrefix) return Result::failure(resolvedPrefix.error());
    const auto& prefix = resolvedPrefix.value();

    const std::string presentKey = prefix + ".present";
    const std::string deviceKey = prefix + ".device";
    const std::string kindKey = prefix + ".frame_kind";
    const std::string pixelFormatKey = prefix + ".pixel_format";
    const std::string widthKey = prefix + ".width";
    const std::string heightKey = prefix + ".height";
    if (!node.options.has(presentKey) ||
        node.options.value(presentKey) != "1" ||
        !node.options.has(deviceKey) ||
        !node.options.has(kindKey) ||
        !node.options.has(pixelFormatKey) ||
        node.options.value(pixelFormatKey).empty() ||
        !node.options.has(widthKey) || !node.options.has(heightKey)) {
        return Result::failure(::media::ErrorInfo::notInitialized(
            "frame payload producer lacks a complete planner frame contract"));
    }
    const auto positiveDimension = [&](const std::string& key) {
        int value = 0;
        const auto text = node.options.value(key);
        const auto parsed = std::from_chars(
            text.data(), text.data() + text.size(), value);
        return parsed.ec == std::errc{} &&
            parsed.ptr == text.data() + text.size() && value > 0;
    };
    if (!positiveDimension(widthKey) || !positiveDimension(heightKey)) {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "frame payload producer has invalid planner dimensions"));
    }
    const auto frameKind = node.options.value(kindKey);
    const auto device = node.options.value(deviceKey);
    MediaFrameCreditAllocationScope allocationScope;
    if (frameKind == "software") {
        if (device != "software") {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "software frame credit conflicts with planner device"));
        }
        allocationScope = MediaFrameCreditAllocationScope::EngineLogicalBytes;
    } else if (frameKind == "hardware" ||
               frameKind == "hardware_mapped") {
        if (device.empty() || device == "software" || device == "unknown") {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "device frame credit conflicts with planner device"));
        }
        allocationScope =
            MediaFrameCreditAllocationScope::ExternalDeviceObservedOnly;
    } else {
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "frame payload producer has unsupported planner frame kind"));
    }
    return Result::success(MediaFrameCreditContract{
        allocationScope, maximumLogicalBytes, 1,
        prefix + "+opened-frame-readback+device=" + device});
}

} // namespace media::ffmpeg::graph
