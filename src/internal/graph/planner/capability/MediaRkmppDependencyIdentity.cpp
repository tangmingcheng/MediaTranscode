#include "internal/graph/planner/capability/MediaRkmppDependencyIdentity.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavutil/avutil.h>
}

namespace media::ffmpeg::graph {

bool MediaRkmppDependencyIdentity::matchesRuntime() noexcept
{
    // This audited revision preserves last_pkt close ownership and adds the
    // MPP picture-associated opaque contract. Deployment also checks MPP hashes.
    if (std::string_view(av_version_info()) != "92aec19") return false;
    const std::string_view configuration(avutil_configuration());
    return configuration == avcodec_configuration() &&
           configuration == avfilter_configuration();
}

} // namespace media::ffmpeg::graph
