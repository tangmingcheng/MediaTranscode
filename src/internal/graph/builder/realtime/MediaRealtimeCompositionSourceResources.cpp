#include "internal/graph/builder/realtime/MediaRealtimeCompositionGraphBuilder.h"

#include <new>
#include <unordered_set>

namespace media::ffmpeg::graph {
::media::Result<std::vector<MediaRealtimeCompositionSourceResources>>
MediaRealtimeCompositionTopology::planSourceResources() const
try {
    using Result = ::media::Result<std::vector<MediaRealtimeCompositionSourceResources>>;
    const auto invalid = [](const char* message) {
        return Result::failure(::media::ErrorInfo::invalidArgument(message));
    };
    if (targets_.size() != options_.sources.size() || targets_.empty())
        return invalid("Composition resource facts require every source target");
    std::unordered_set<std::uint32_t> assigned;
    const auto add = [&](const std::vector<MediaNodeId>& nodes) {
        for (const auto id : nodes)
            if (!graph_.findNode(id) || !assigned.insert(id.value).second) return false;
        return true;
    };
    if (!add(output_.processingMembers))
        return invalid("Composition output membership is missing or repeated");
    std::vector<MediaRealtimeCompositionSourceResources> result;
    for (std::size_t i = 0; i < targets_.size(); ++i) {
        if (targets_[i].sourceIndex != i || !add(targets_[i].sourceMembers))
            return invalid("Composition source membership overlaps or has an invalid identity");
        auto facts = MediaRealtimeCompositionSourceResourcesPlanner::plan(graph_, options_.sources[i], targets_[i]);
        if (!facts) return Result::failure(facts.error());
        result.push_back(std::move(facts).value());
    }
    if (assigned.size() != graph_.nodes().size())
        return invalid("Composition resource membership does not cover the final graph");
    return Result::success(std::move(result));
} catch (const std::bad_alloc&) {
    return ::media::Result<std::vector<MediaRealtimeCompositionSourceResources>>::failure(
        ::media::ErrorInfo::allocationFailed("composition source membership"));
}
} // namespace media::ffmpeg::graph
