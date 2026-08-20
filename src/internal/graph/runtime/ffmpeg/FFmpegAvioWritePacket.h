#pragma once

extern "C" {
#include <libavformat/version_major.h>
}

#include <cstdint>

namespace media::ffmpeg {

#if LIBAVFORMAT_VERSION_MAJOR >= 61
using FFmpegAvioWritePacketByte = const std::uint8_t;
#else
using FFmpegAvioWritePacketByte = std::uint8_t;
#endif

} // namespace media::ffmpeg
