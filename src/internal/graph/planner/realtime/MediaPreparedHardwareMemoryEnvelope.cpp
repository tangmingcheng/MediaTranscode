#include "internal/graph/planner/realtime/MediaPreparedHardwareMemoryEnvelope.h"

#include "internal/graph/utils/MediaCheckedArithmetic.h"

namespace media::ffmpeg::graph {

::media::Status MediaPreparedHardwareMemoryEnvelope::validate() const
{
    if (backend.empty() || authority.empty() || allocations.empty() ||
        maximumDeviceAndDriverBytes == 0) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "prepared hardware memory envelope is incomplete"));
    }
    std::uint64_t accounted = 0;
    for (const auto& allocation : allocations) {
        if (allocation.role == MediaHardwareMemoryAllocationRole::Unknown ||
            allocation.maximumPoolSurfaces == 0 ||
            allocation.allocationBytesPerSurface == 0 ||
            allocation.rowStrideBytes == 0 || allocation.alignmentBytes == 0 ||
            allocation.authority.empty()) {
            return ::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "prepared hardware allocation lacks a hard bound or authority"));
        }
        auto surfaceBytes = MediaCheckedArithmetic::multiply(
            allocation.maximumPoolSurfaces,
            allocation.allocationBytesPerSurface,
            "hardware surface allocation bytes");
        auto allocationBytes = surfaceBytes
            ? MediaCheckedArithmetic::add(
                  surfaceBytes.value(),
                  allocation.maximumDriverOverheadBytes,
                  "hardware allocation and driver overhead")
            : surfaceBytes;
        auto total = allocationBytes
            ? MediaCheckedArithmetic::add(
                  accounted, allocationBytes.value(),
                  "prepared hardware memory total")
            : allocationBytes;
        if (!total) return ::media::Status::failure(total.error());
        accounted = total.value();
    }
    return accounted == maximumDeviceAndDriverBytes
        ? ::media::Status::success()
        : ::media::Status::failure(::media::ErrorInfo::invalidArgument(
              "prepared hardware memory total does not match allocations"));
}

} // namespace media::ffmpeg::graph
