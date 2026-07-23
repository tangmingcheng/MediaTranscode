#include "internal/graph/planner/capability/MediaEncoderPacketLayoutCapabilityProvider.h"

namespace media::ffmpeg::graph {

std::optional<MediaEncodedPacketLayout>
MediaEncoderPacketLayoutCapabilityProvider::find(
    std::string_view ffmpegEncoderName) noexcept
{
    if (ffmpegEncoderName == "h264_nvenc" ||
        ffmpegEncoderName == "libx264" ||
        ffmpegEncoderName == "libx264rgb") {
        return MediaEncodedPacketLayout::startCodeDelimited();
    }
    return std::nullopt;
}

} // namespace media::ffmpeg::graph
