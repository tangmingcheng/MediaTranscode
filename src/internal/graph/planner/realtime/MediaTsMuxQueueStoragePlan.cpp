#include "internal/graph/planner/realtime/MediaTsMuxQueueStoragePlan.h"
#include "internal/graph/runtime/buffer/MediaBufferRef.h"
#include "internal/graph/runtime/resource/MediaNetworkMemoryResource.h"
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaTsMuxQueueStoragePlan> MediaTsMuxQueueStoragePlanner::plan(
    std::uint64_t maximumQueuedBytes)
{
    using Result = ::media::Result<MediaTsMuxQueueStoragePlan>;
    // Every queued protocol batch owns at least one complete MPEG-TS packet.
    const auto slots = maximumQueuedBytes / 188;
    if (!slots || slots > (std::numeric_limits<std::size_t>::max)() / sizeof(MediaBufferRef))
        return Result::failure(::media::ErrorInfo::invalidArgument("TS mux queue storage is not representable"));
    auto bytes = MediaNetworkMemoryResource::allocationFootprint(
        static_cast<std::size_t>(slots) * sizeof(MediaBufferRef), alignof(MediaBufferRef));
    if (!bytes) return Result::failure(bytes.error());
    return Result::success({maximumQueuedBytes, static_cast<std::size_t>(slots), bytes.value()});
}
::media::Status MediaTsMuxQueueStoragePlanner::validate(const MediaTsMuxQueueStoragePlan& product)
{
    auto expected = plan(product.maximumQueuedBytes);
    if (!expected) return ::media::Status::failure(expected.error());
    if (expected.value() != product) return ::media::Status::failure(
        ::media::ErrorInfo::invalidArgument("TS mux queue storage differs from its exact allocation geometry"));
    return ::media::Status::success();
}
} // namespace media::ffmpeg::graph
