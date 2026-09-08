#pragma once

#include "internal/graph/protocol/rtp/MediaScheduledRtpPacketizationMode.h"
#include "media_transcode/Result.h"

#include <string_view>

namespace media::ffmpeg::graph {

class MediaScheduledRtpPacketizationModeCodec final {
public:
    static std::string_view encode(
        MediaScheduledRtpPacketizationMode mode) noexcept;

    static ::media::Result<MediaScheduledRtpPacketizationMode> decode(
        std::string_view value);
};

} // namespace media::ffmpeg::graph
