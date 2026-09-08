#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

::media::Result<bool> parseMediaVideoLineageCopyOpaqueOption(
    const MediaNodeOptions* options,
    const std::string_view optionName)
{
    if (!options) {
        return ::media::Result<bool>::success(false);
    }
    const bool hasCapacity = options->has("video.lineage.capacity");
    const bool hasIdentity = options->has("video.lineage.identity");
    const std::string copyOpaqueOption(optionName);
    const bool hasCopyOpaque = options->has(copyOpaqueOption);
    if (!hasCapacity && !hasIdentity && !hasCopyOpaque) {
        return ::media::Result<bool>::success(false);
    }
    if (!hasCapacity || !hasCopyOpaque) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage COPY_OPAQUE requires the complete planned capacity contract"));
    }

    const std::string capacityText = options->value("video.lineage.capacity");
    std::size_t capacity = 0;
    const auto parsed = std::from_chars(
        capacityText.data(), capacityText.data() + capacityText.size(), capacity);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != capacityText.data() + capacityText.size() ||
        capacity == 0 ||
        (options->value(copyOpaqueOption) != "1" &&
         options->value(copyOpaqueOption) != "0")) {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage transport requires positive capacity and explicit COPY_OPAQUE value"));
    }
    return ::media::Result<bool>::success(
        options->value(copyOpaqueOption) == "1");
}

} // namespace media::ffmpeg::graph
