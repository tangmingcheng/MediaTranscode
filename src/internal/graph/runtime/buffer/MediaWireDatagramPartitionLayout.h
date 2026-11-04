#pragma once

#include "internal/graph/planner/realtime/MediaDatagramShapingPlan.h"

namespace media::ffmpeg::graph {

// Value-only cursor shared by storage sizing and actual partition construction.
// No protocol state or storage ownership is changed by next().
struct MediaWireDatagramPartitionLayout final {
    std::uint64_t datagrams = 0;
    std::uint64_t bytes = 0;
    std::optional<MediaRunningTime> deadline;

    ::media::Result<MediaWireDatagramPartitionLayout> next(
        const MediaDatagramBatchPlan& plan, std::uint64_t datagramBytes,
        MediaRunningTime datagramDeadline) const
    {
        using Result = ::media::Result<MediaWireDatagramPartitionLayout>;
        if (!plan.maximumDatagrams || !plan.maximumBytes || !datagramBytes ||
            datagramBytes > plan.maximumBytes || datagrams > plan.maximumDatagrams ||
            bytes > plan.maximumBytes ||
            (datagrams == 0) != (bytes == 0) ||
            (datagrams != 0) != deadline.has_value()) {
            return Result::failure(::media::ErrorInfo::invalidArgument(
                "wire partition layout requires valid planner bounds and a complete cursor"));
        }
        const bool begin = !deadline || *deadline != datagramDeadline ||
            datagrams == plan.maximumDatagrams ||
            bytes > plan.maximumBytes - datagramBytes;
        return Result::success(MediaWireDatagramPartitionLayout{
            begin ? 1 : datagrams + 1,
            begin ? datagramBytes : bytes + datagramBytes,
            datagramDeadline});
    }
};

} // namespace media::ffmpeg::graph
