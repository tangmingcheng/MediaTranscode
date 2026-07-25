#pragma once

#include "internal/graph/model/MediaEncodedPacketLayout.h"

#include <optional>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaEncoderPacketLayoutCapabilityProvider final {
public:
    static std::optional<MediaEncodedPacketLayout> find(
        std::string_view ffmpegEncoderName) noexcept;

    MediaEncoderPacketLayoutCapabilityProvider() = delete;
};

} // namespace media::ffmpeg::graph
