#include "internal/graph/planner/capability/MediaVideoSourceIsolationAdapter.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
}
#include <string_view>

namespace media::ffmpeg::graph {
::media::Result<MediaVideoFilterIsolationEvidence> MediaVideoSourceIsolationAdapter::inspect(MediaHardwareDeviceKind device)
{
    using Result = ::media::Result<MediaVideoFilterIsolationEvidence>;
    if (device != MediaHardwareDeviceKind::RKMPP ||
        std::string_view(av_version_info()) != "d90e3a1" || !avfilter_get_by_name("scale_rkrga"))
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
        "ffmpeg-rockchip/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf:rkrga_common.init_hwframes_ctx/query_frame;hwcontext_rkmpp",
        "ffmpeg-rockchip/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf:rkrga_common.ff_rkrga_filter_frame async_depth=0 fence wait",
        1,
        "ffmpeg-rockchip/d90e3a1c18d7929383cf88c1b3da2e2d1c966cbf:rkrga_common src frame cache until clear_unused"});
}
} // namespace media::ffmpeg::graph
