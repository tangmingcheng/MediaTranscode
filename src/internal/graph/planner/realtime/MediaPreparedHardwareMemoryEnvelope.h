#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {

enum class MediaHardwareMemoryAllocationRole : std::uint8_t {
    Unknown = 0,
    DecoderSurfacePool = 1,
    FilterSurfacePool = 2,
    EncoderSurfacePool = 3,
    DriverAllocation = 4
};

struct MediaPreparedHardwareMemoryAllocation final {
    MediaHardwareMemoryAllocationRole role =
        MediaHardwareMemoryAllocationRole::Unknown;
    std::uint64_t maximumPoolSurfaces = 0;
    std::uint64_t allocationBytesPerSurface = 0;
    std::uint64_t rowStrideBytes = 0;
    std::uint64_t alignmentBytes = 0;
    std::uint64_t maximumDriverOverheadBytes = 0;
    std::string authority;
};

struct MediaPreparedHardwareMemoryEnvelope final {
    std::string backend;
    std::string authority;
    std::vector<MediaPreparedHardwareMemoryAllocation> allocations;
    std::uint64_t maximumDeviceAndDriverBytes = 0;

    ::media::Status validate() const;
};

} // namespace media::ffmpeg::graph
