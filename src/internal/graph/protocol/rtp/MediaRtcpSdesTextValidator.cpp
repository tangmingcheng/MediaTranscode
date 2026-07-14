#include "internal/graph/protocol/rtp/MediaRtcpSdesTextValidator.h"
#include "internal/graph/protocol/MediaUtf8TextValidator.h"

namespace media::ffmpeg::graph {
::media::Status MediaRtcpSdesTextValidator::validateCname(
    std::string_view cname)
{
    return MediaUtf8TextValidator::validateNonControlText(
        cname, 255, "RTCP SDES CNAME");
}

} // namespace media::ffmpeg::graph
