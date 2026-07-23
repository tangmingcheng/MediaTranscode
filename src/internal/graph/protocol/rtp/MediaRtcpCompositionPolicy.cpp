#include "internal/graph/protocol/rtp/MediaRtcpCompositionPolicy.h"

namespace media::ffmpeg::graph {

::media::Result<std::string> serializeMediaRtcpCompositionMode(
    MediaRtcpCompositionMode mode)
{
    switch (mode) {
    case MediaRtcpCompositionMode::StrictCompoundRfc3550:
        return ::media::Result<std::string>::success("strict_compound_rfc3550");
    case MediaRtcpCompositionMode::ReducedSizeRfc5506:
        return ::media::Result<std::string>::success("reduced_size_rfc5506");
    }
    return ::media::Result<std::string>::failure(
        ::media::ErrorInfo::invalidArgument("unsupported planned RTCP composition mode"));
}

::media::Result<MediaRtcpCompositionMode> parseMediaRtcpCompositionMode(
    std::string_view value)
{
    if (value == "strict_compound_rfc3550") {
        return ::media::Result<MediaRtcpCompositionMode>::success(
            MediaRtcpCompositionMode::StrictCompoundRfc3550);
    }
    if (value == "reduced_size_rfc5506") {
        return ::media::Result<MediaRtcpCompositionMode>::success(
            MediaRtcpCompositionMode::ReducedSizeRfc5506);
    }
    return ::media::Result<MediaRtcpCompositionMode>::failure(
        ::media::ErrorInfo::invalidArgument("unsupported planned RTCP composition mode option"));
}

} // namespace media::ffmpeg::graph
