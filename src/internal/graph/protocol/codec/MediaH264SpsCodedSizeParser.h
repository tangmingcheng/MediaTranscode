#pragma once

#include "internal/graph/model/MediaGraphTypes.h"
#include "media_transcode/Result.h"

#include "internal/graph/model/MediaVideoColorRangeFact.h"
#include <optional>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaH264SpsCodedSizeParser final {
public:
    static ::media::Result<MediaSize> parse(
        std::span<const std::uint8_t> sps,
        std::optional<MediaVideoColorRangeFact>* colorRange = nullptr);

private:
    MediaH264SpsCodedSizeParser() = delete;
};

} // namespace media::ffmpeg::graph
