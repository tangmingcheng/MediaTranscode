#include "internal/graph/planner/realtime/MediaVideoCanvasPreparationPlanner.h"
#include "internal/graph/planner/video/MediaVideoCanvasGeometryValidator.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
extern "C" {
#include <libavutil/imgutils.h>
}
#include <limits>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoCanvasPreparationPlan> MediaVideoCanvasPreparationPlanner::plan(
    const MediaVideoCanvasGeometry& geometry, const MediaVideoCanvasRetentionPlan& retention,
    const MediaVideoCanvasAllocation& allocation)
{
    using Result = ::media::Result<MediaVideoCanvasPreparationPlan>;
    if (auto status = MediaVideoCanvasGeometryValidator::validate(geometry); !status)
        return Result::failure(status.error());
    auto pool = MediaEncoderHardwareFramesPoolPlanner::plan(retention.encoderPool);
    if (!pool) return Result::failure(pool.error());
    const auto staging = av_image_get_buffer_size(geometry.softwareFormat,
        geometry.width, geometry.height, 1);
    if (pool.value() != retention.encoderPool || !retention.writableSurfaces ||
        retention.encoderPool.persistentPoolSurfaces != 1 ||
        retention.encoderPool.initialPoolSurfaces - 1 != retention.writableSurfaces ||
        retention.maximumPublishedLeases != retention.writableSurfaces ||
        retention.encoderPool.initialPoolSurfaces > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        !allocation.surfaceBytes || staging <= 0 ||
        allocation.stagingBytes != static_cast<std::uint64_t>(staging))
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Canvas preparation requires topology retention and actual allocation readback"));
    auto retained = MediaCheckedArithmetic::multiply(retention.encoderPool.initialPoolSurfaces,
        allocation.surfaceBytes, "canvas retained image payload");
    // The upload and download verification frames coexist until comparison.
    auto transient = MediaCheckedArithmetic::multiply(2, allocation.stagingBytes,
        "canvas staging image payload");
    if (!retained) return Result::failure(retained.error());
    if (!transient) return Result::failure(transient.error());
    auto total = MediaCheckedArithmetic::add(retained.value(), transient.value(),
        "canvas preparation image payload");
    if (!total) return Result::failure(total.error());
    if (total.value() > std::numeric_limits<std::size_t>::max())
        return Result::failure(::media::ErrorInfo::allocationFailed(
            "Canvas preparation payload exceeds addressable storage"));
    return Result::success(MediaVideoCanvasPreparationPlan(
        {geometry, {retention.writableSurfaces, allocation.surfaceBytes,
            allocation.stagingBytes, retention.maximumPublishedLeases}},
        static_cast<std::size_t>(retained.value()), static_cast<std::size_t>(transient.value())));
}
} // namespace media::ffmpeg::graph
