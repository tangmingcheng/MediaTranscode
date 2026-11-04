#pragma once
#include <cstdint>
#include "internal/graph/runtime/resource/MediaSharedAllocationGeometry.h"
namespace media::ffmpeg::graph {
struct MediaRtpProtocolStoragePlan final {
    std::uint64_t rtpEndpointId;
    std::uint64_t rtcpEndpointId;
    std::uint64_t recordCapacity;
    MediaSharedAllocationGeometry recordAllocation;
};
} // namespace media::ffmpeg::graph
