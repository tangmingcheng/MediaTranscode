#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace media::ffmpeg::graph {

class FFmpegAvioOutputByteSinkBackend {
public:
    static ::media::Result<std::unique_ptr<FFmpegAvioOutputByteSinkBackend>> open(
        std::string url,
        int writeFlags);

    virtual ~FFmpegAvioOutputByteSinkBackend() = default;

    virtual void write(std::span<const std::uint8_t> bytes) = 0;
    virtual void flush() = 0;
    virtual int error() const noexcept = 0;
    virtual int close() noexcept = 0;
};

} // namespace media::ffmpeg::graph
