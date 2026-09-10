#pragma once

#include "media_transcode/Result.h"
#include <memory>

extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
}

namespace media::ffmpeg::graph {
class MediaGraphPayloadCreditLease;

// Bind credit to the actual submitted allocation references, not COPY_OPAQUE
// metadata that codecs and filters propagate to newly produced allocations.
::media::Status retainMediaFfmpegPayload(
    AVPacket& packet, const std::shared_ptr<MediaGraphPayloadCreditLease>& credit);
::media::Status retainMediaFfmpegPayload(
    AVFrame& frame, const std::shared_ptr<MediaGraphPayloadCreditLease>& credit);
} // namespace media::ffmpeg::graph
