#pragma once

#include "internal/graph/time/MediaRunningTime.h"
#include "media_transcode/Result.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace media::ffmpeg::graph {

class MediaTsDatagramSink {
public:
    virtual ~MediaTsDatagramSink() = default;

    virtual ::media::Result<std::size_t> write(
        std::span<const std::uint8_t> completeTsPackets,
        MediaRunningTime emitOnMaster) = 0;
    virtual ::media::Status flush() = 0;
    virtual ::media::Status close() = 0;
    virtual void abort() noexcept = 0;
};

} // namespace media::ffmpeg::graph
