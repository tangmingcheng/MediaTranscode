#pragma once

#include "internal/graph/protocol/rtp/MediaRtpDepacketizer.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"

#include <memory>

namespace media::ffmpeg::graph {

class MediaRawRtpStreamDescriptorFactory final {
public:
    static ::media::Result<std::unique_ptr<FFmpegFormatContextBuffer>> create(
        const MediaRtpDepacketizerConfig& config);

private:
    MediaRawRtpStreamDescriptorFactory() = delete;
};

} // namespace media::ffmpeg::graph
