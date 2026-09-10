#include "internal/graph/planner/capability/MediaVideoSourceIsolationAdapter.h"
#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"
extern "C" {
#include <libavfilter/avfilter.h>
}
#include <string>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoFilterIsolationEvidence> MediaVideoSourceIsolationAdapter::inspect(MediaHardwareDeviceKind device)
{
    using Result = ::media::Result<MediaVideoFilterIsolationEvidence>;
    if (device != MediaHardwareDeviceKind::RKMPP ||
        !MediaRkmppDependencyIdentity::matchesRuntime() || !avfilter_get_by_name("scale_rkrga"))
        return Result::failure(::media::ErrorInfo::hardwareUnavailable(
            "shared source copy requires a verified independent allocation and synchronous completion implementation"));
    // Verified deployed FFmpeg commit. Identity scaling does not bypass RGA:
    // init_hwframes_ctx allocates an independent RKMPP pool; async_depth=0
    // waits the submitted fence before publishing the independent output.
    // The RGA source cache can retain its input until clear_unused; its slot
    // is part of the separately planned decoder-producer retention budget.
    return Result::success({"scale_rkrga=w=iw:h=ih:async_depth=0",
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
} // namespace media::ffmpeg::graph
