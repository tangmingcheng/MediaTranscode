#include "internal/graph/planner/realtime/MediaVideoOutputPreparationPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <limits>
extern "C" {
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {
::media::Result<MediaVideoOutputPreparationPlan> MediaVideoOutputPreparationPlanner::plan(
    const MediaVideoCanvasRetentionPlan& retention,
    const MediaVideoCanvasAllocation& allocation)
{
    using Result = ::media::Result<MediaVideoOutputPreparationPlan>;
    auto pool = MediaEncoderHardwareFramesPoolPlanner::plan(retention.encoderPool);
    if (!pool) return Result::failure(pool.error());
    if (pool.value() != retention.encoderPool || !retention.writableSurfaces ||
        retention.encoderPool.persistentPoolSurfaces != 1 ||
        retention.encoderPool.initialPoolSurfaces - 1 != retention.writableSurfaces ||
        retention.maximumPublishedLeases != retention.writableSurfaces ||
        retention.encoderPool.initialPoolSurfaces > static_cast<std::uint64_t>((std::numeric_limits<int>::max)()) ||
        !allocation.surfaceBytes || !allocation.stagingBytes)
        return Result::failure(::media::ErrorInfo::invalidArgument(
            "Output preparation requires planned canvas retention and actual same-device allocation readback"));
    auto unit = MediaCheckedArithmetic::add(allocation.surfaceBytes, sizeof(AVFrame),
        "prepared output surface and public frame header");
    if (!unit) return Result::failure(unit.error());
    auto bytes = MediaCheckedArithmetic::multiply(retention.encoderPool.initialPoolSurfaces,
        unit.value(), "frozen composition output pool preparation storage");
    if (!bytes || bytes.value() > (std::numeric_limits<std::size_t>::max)())
        return Result::failure(bytes ? ::media::ErrorInfo::invalidArgument(
            "Output preparation storage exceeds the platform size range") : bytes.error());
    return Result::success(MediaVideoOutputPreparationPlan(
        retention.encoderPool, allocation, static_cast<std::size_t>(bytes.value())));
}
} // namespace media::ffmpeg::graph
