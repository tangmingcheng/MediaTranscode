#pragma once

#include <string_view>

namespace media::ffmpeg::graph {

// Version-scoped source evidence. Deployment still verifies the complete
// shared-library set by hashes; configuration equality is not a content hash.
class MediaRkmppDependencyIdentity final {
public:
    static bool matchesRuntime() noexcept;
    static constexpr std::string_view sourceRevision =
        "ffmpeg-rockchip/ab1e61adaa21ff129caa8e01a1198156044567ed";
};

} // namespace media::ffmpeg::graph
