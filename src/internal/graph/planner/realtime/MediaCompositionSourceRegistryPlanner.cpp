#include "internal/graph/planner/realtime/MediaCompositionSourceRegistryPlanner.h"
#include "internal/graph/planner/realtime/MediaDecoderOpaqueRetentionPlanner.h"
#include "internal/graph/utils/MediaCheckedArithmetic.h"
#include <algorithm>
namespace media::ffmpeg::graph {
::media::Result<std::uint64_t> MediaCompositionSourceRegistryPlanner::plan(const MediaVideoSourcePlan& video)
{
    using Result = ::media::Result<std::uint64_t>;
    if (!video.decoder.preparedOpaqueRetention)
        return Result::failure(::media::ErrorInfo::notInitialized("Decoded source lacks prepared opaque retention facts"));
    auto capacity = MediaDecoderOpaqueRetentionPlanner::registryCapacity(*video.decoder.preparedOpaqueRetention);
    if (!capacity) return capacity;
    if (video.filterActive) {
        if (!video.filterExecution || !video.filterExecution->valid() || !video.filterExecution->inputConsumption ||
            video.filterExecution->output.filterDescription != video.filter.filterName)
            return Result::failure(::media::ErrorInfo::notInitialized("Source registry requires its prepared filter execution contract"));
        // Filter pending input and receive output coexist with its declared retained inputs.
        auto filter = MediaCheckedArithmetic::add(video.filterExecution->output.maximumRetainedInputFrames,
            2, "source filter opaque progress slots");
        if (!filter) return filter;
        return Result::success((std::max)(capacity.value(), filter.value()));
    }
    return capacity;
}
}
