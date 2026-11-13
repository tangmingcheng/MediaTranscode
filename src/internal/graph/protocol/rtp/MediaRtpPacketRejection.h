#pragma once
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {
// A prepared rejection preserves the original error and its existing fragment
// cleanup requirement. Inspection is const; cleanup occurs only at commit.
struct MediaRtpPacketRejection final {
    ::media::ErrorInfo error;
    bool discardFragment;
};
} // namespace media::ffmpeg::graph
