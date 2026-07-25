#include "internal/graph/sync/lineage/MediaVideoLineageCopyOpaqueOption.h"

#include <charconv>
#include <cstddef>
#include <string>

namespace media::ffmpeg::graph {

::media::Result<bool> parseMediaVideoLineageCopyOpaqueOption(
    const MediaNodeOptions* options)
{
    if (!options) {
        return ::media::Result<bool>::success(false);
    }
    const bool hasCapacity = options->has("video.lineage.capacity");
    const bool hasIdentity = options->has("video.lineage.identity");
    const bool hasCopyOpaque = options->has("video.lineage.copy_opaque");
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
        capacity == 0 || options->value("video.lineage.copy_opaque") != "1") {
        return ::media::Result<bool>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Codec lineage COPY_OPAQUE requires positive capacity and explicit value 1"));
    }
    return ::media::Result<bool>::success(true);
}

} // namespace media::ffmpeg::graph
