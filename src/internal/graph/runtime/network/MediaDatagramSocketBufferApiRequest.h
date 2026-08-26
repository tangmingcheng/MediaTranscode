#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

enum class MediaDatagramSocketBufferApiAccounting {
    Exact = 1,
    LinuxDoubled = 2
};

class MediaDatagramSocketBufferApiRequest final {
public:
    static ::media::Result<std::uint64_t> fromTargetEffective(
        std::uint64_t targetEffectiveBytes,
        MediaDatagramSocketBufferApiAccounting accounting) noexcept;
};

} // namespace media::ffmpeg::graph
