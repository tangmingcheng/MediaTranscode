#pragma once

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaOutputTransportKind : std::uint8_t {
    UdpDatagrams = 0,
    RtpAvp = 1
};

} // namespace media::ffmpeg::graph
