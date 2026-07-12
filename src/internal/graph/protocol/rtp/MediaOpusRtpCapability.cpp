#include "internal/graph/protocol/rtp/MediaOpusRtpCapability.h"

namespace media::ffmpeg::graph {

::media::Status validateOpusRtpMappingFamilyZeroChannels(int channels)
{
    if (channels < 1 || channels > 2) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "Opus RTP mapping family 0 requires one or two channels"));
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
