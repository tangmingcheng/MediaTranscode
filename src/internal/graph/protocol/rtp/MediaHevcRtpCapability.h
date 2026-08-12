#pragma once

#include "internal/graph/protocol/rtp/MediaRtpFmtp.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

::media::Status validateHevcRtpNonInterleavedFmtp(
    const MediaRtpFmtpParameters& parameters);

} // namespace media::ffmpeg::graph
