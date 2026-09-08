#pragma once

#include "media_transcode/Result.h"

#include <cstdint>
#include <string>

struct AVCodecContext;

namespace media::ffmpeg::graph {

struct MediaEncoderPacketCapacityPreflightResult final {
    std::uint64_t maximumAccessUnitPayloadBytes;
    std::string authority;
};

class MediaEncoderPacketCapacityPreflight final {
public:
    static ::media::Result<MediaEncoderPacketCapacityPreflightResult> derive(
        const AVCodecContext& context,
        const std::string& backend,
        std::uint64_t rateControlBurstBytes);

private:
    MediaEncoderPacketCapacityPreflight() = delete;
};

} // namespace media::ffmpeg::graph
