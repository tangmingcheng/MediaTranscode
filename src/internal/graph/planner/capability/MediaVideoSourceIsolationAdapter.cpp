#include "internal/graph/planner/capability/MediaVideoSourceIsolationAdapter.h"
#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/opt.h>
}
#include <string>
#include <string_view>
#include <utility>

namespace media::ffmpeg::graph {
namespace {
::media::Result<MediaVideoFilterIsolationEvidence> evidence(std::string description)
{
    using Result = ::media::Result<MediaVideoFilterIsolationEvidence>;
    if (description.empty() || !MediaRkmppDependencyIdentity::matchesRuntime() || !avfilter_get_by_name("scale_rkrga"))
        return Result::failure(::media::ErrorInfo::hardwareUnavailable(
            "shared source copy requires a verified independent allocation and synchronous completion implementation"));
    // Verified deployed FFmpeg commit. Identity scaling does not bypass RGA:
    // init_hwframes_ctx allocates an independent RKMPP pool; async_depth=0
    // waits the submitted fence before publishing the independent output.
    // The RGA source cache can retain its input until clear_unused; its slot
    // is part of the separately planned decoder-producer retention budget.
    return Result::success({std::move(description),
        MediaVideoFilterAllocation::IndependentOutputPool,
        MediaVideoFilterCompletion::SynchronousOnReturn,
        std::string(MediaRkmppDependencyIdentity::sourceRevision) +
            ":rkrga_common.init_hwframes_ctx/query_frame;hwcontext_rkmpp",
        std::string(MediaRkmppDependencyIdentity::sourceRevision) +
            ":rkrga_common.ff_rkrga_filter_frame async_depth=0 fence wait",
        1,
        std::string(MediaRkmppDependencyIdentity::sourceRevision) +
            ":rkrga_common src frame cache until clear_unused"});
}
} // namespace

::media::Result<MediaVideoFilterIsolationEvidence> MediaVideoSourceIsolationAdapter::inspect(MediaHardwareDeviceKind device)
{
    if (device != MediaHardwareDeviceKind::RKMPP)
        return ::media::Result<MediaVideoFilterIsolationEvidence>::failure(::media::ErrorInfo::hardwareUnavailable(
            "Source isolation requires an admitted hardware implementation"));
    return evidence("scale_rkrga=w=iw:h=ih:async_depth=0");
}

::media::Result<MediaVideoFilterIsolationEvidence> MediaVideoSourceIsolationAdapter::inspect(
    const AVFilterGraph& graph, std::string description)
{
    using Result = ::media::Result<MediaVideoFilterIsolationEvidence>;
    const AVFilterContext* filter = nullptr;
    for (unsigned index = 0; index < graph.nb_filters; ++index) {
        const auto* node = graph.filters[index];
        if (!node || !node->filter || !node->filter->name)
            return Result::failure(::media::ErrorInfo::notInitialized("Source isolation graph implementation is absent"));
        const std::string_view name(node->filter->name);
        if (name == "buffer" || name == "buffersink") continue;
        if (filter || name != "scale_rkrga")
            return Result::failure(::media::ErrorInfo::unsupported("Source isolation requires one admitted RGA producer"));
        filter = node;
    }
    std::int64_t depth = -1;
    if (!filter || !filter->priv || av_opt_get_int(filter->priv, "async_depth", 0, &depth) < 0 || depth != 0)
        return ::media::Result<MediaVideoFilterIsolationEvidence>::failure(::media::ErrorInfo::unsupported(
            "Prepared source isolation requires scale_rkrga with authoritative zero async depth"));
    return evidence(std::move(description));
}
} // namespace media::ffmpeg::graph
