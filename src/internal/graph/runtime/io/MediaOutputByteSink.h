#pragma once

#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaOutputByteSink {
public:
    virtual ~MediaOutputByteSink() = default;

    virtual ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> bytes) = 0;
    virtual ::media::Status flush() = 0;
    virtual ::media::Status close() = 0;
};

} // namespace media::ffmpeg::graph
