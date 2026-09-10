#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
}

namespace media::ffmpeg::graph {

bool MediaRkmppDependencyIdentity::matchesRuntime() noexcept
{
    // This revision releases the decoder's private last_pkt on close. The
    // parent d90e3a1 cannot authorize the same ownership/retirement contract.
    if (std::string_view(av_version_info()) != "ab1e61a") return false;
    const std::string_view configuration(avutil_configuration());
    return configuration == avcodec_configuration() &&
           configuration == avfilter_configuration();
}

} // namespace media::ffmpeg::graph
