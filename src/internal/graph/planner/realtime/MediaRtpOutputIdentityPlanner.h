#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace media::ffmpeg::graph {

class MediaRtpOutputIdentityPlanner final {
public:
    static std::uint32_t stableNumeric(std::string_view identity) noexcept;
    static std::string cname(std::string_view sessionIdentity);

private:
    MediaRtpOutputIdentityPlanner() = delete;
};

} // namespace media::ffmpeg::graph
