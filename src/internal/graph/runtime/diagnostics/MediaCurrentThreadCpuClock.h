#pragma once

#include "media_transcode/Result.h"

#include <cstdint>

namespace media::ffmpeg::graph {

class MediaCurrentThreadCpuClock final {
public:
    static ::media::Result<std::uint64_t> nowNanoseconds() noexcept;

private:
    MediaCurrentThreadCpuClock() = delete;
};

} // namespace media::ffmpeg::graph
