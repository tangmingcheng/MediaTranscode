#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaRtpDepacketizerFactory final {
public:
    static ::media::Status validate(const MediaRtpDepacketizerConfig& config);
    static ::media::Result<std::unique_ptr<MediaRtpDepacketizer>> create(
        const MediaRtpDepacketizerConfig& config);

private:
    MediaRtpDepacketizerFactory() = delete;
};

} // namespace media::ffmpeg::graph
