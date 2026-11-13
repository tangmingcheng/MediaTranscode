#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

// Version-scoped source evidence. Deployment still verifies the complete
// shared-library set by hashes; configuration equality is not a content hash.
class MediaRkmppDependencyIdentity final {
public:
    static bool matchesRuntime() noexcept;
    static constexpr std::string_view sourceRevision =
        "ffmpeg-rockchip/92aec19cedcfc55cd5ef0a8b2fff1ca998bcb696";
};

} // namespace media::ffmpeg::graph
