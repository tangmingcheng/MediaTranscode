#pragma once

#include "internal/graph/model/MediaHardwareDescriptor.h"
#include "internal/graph/model/MediaZeroCopyPolicy.h"

namespace media::ffmpeg::graph {

struct MediaZeroCopyCapability {
    MediaHardwareDeviceKind deviceKind = MediaHardwareDeviceKind::Unknown;
    MediaInteropKind interopKind = MediaInteropKind::None;

    bool supportsImport = false;
    bool supportsExport = false;
    bool supportsMapping = false;
    bool supportsSharedFrames = false;

    constexpr bool usable() const noexcept
    {
        return deviceKind != MediaHardwareDeviceKind::Unknown &&
               interopKind != MediaInteropKind::None;
    }
};

} // namespace media::ffmpeg::graph
