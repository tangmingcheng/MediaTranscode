#include "internal/graph/sync/lineage/MediaVideoLineageStagePreparation.h"

#include <charconv>
#include <string>

namespace media::ffmpeg::graph {

::media::Result<std::optional<std::size_t>>
prepareMediaVideoLineageStageCapacity(
    const MediaNode& node,
    std::string_view expectedIdentity)
{
    const bool hasCapacity = node.options.has("video.lineage.capacity");
    const bool hasIdentity = node.options.has("video.lineage.identity");
    if (!hasCapacity && !hasIdentity) {
        return ::media::Result<std::optional<std::size_t>>::success(std::nullopt);
    }
    if (!hasCapacity || !hasIdentity || expectedIdentity.empty() ||
        node.options.value("video.lineage.identity") != expectedIdentity) {
        return ::media::Result<std::optional<std::size_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video lineage stage requires complete exact planner identity and capacity"));
    }

    const std::string capacityText =
        node.options.value("video.lineage.capacity");
    std::size_t capacity = 0;
    const auto parsed = std::from_chars(
        capacityText.data(), capacityText.data() + capacityText.size(), capacity);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != capacityText.data() + capacityText.size() ||
        capacity == 0) {
        return ::media::Result<std::optional<std::size_t>>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Video lineage stage requires positive planned capacity"));
    }
    return ::media::Result<std::optional<std::size_t>>::success(capacity);
}

::media::Result<std::shared_ptr<MediaCodecLineageRegistry>>
prepareMediaVideoLineageStage(
    const MediaNode& node,
    std::string_view expectedIdentity)
{
    auto capacity = prepareMediaVideoLineageStageCapacity(node, expectedIdentity);
    if (!capacity) {
        return ::media::Result<std::shared_ptr<MediaCodecLineageRegistry>>::failure(
            capacity.error());
    }
    if (!capacity.value()) {
        return ::media::Result<std::shared_ptr<MediaCodecLineageRegistry>>::success(
            nullptr);
    }
    auto registry = MediaCodecLineageRegistry::create(*capacity.value());
    if (!registry) {
        return ::media::Result<std::shared_ptr<MediaCodecLineageRegistry>>::failure(
            registry.error());
    }
    return ::media::Result<std::shared_ptr<MediaCodecLineageRegistry>>::success(
        std::make_shared<MediaCodecLineageRegistry>(
            std::move(registry).value()));
}

} // namespace media::ffmpeg::graph
