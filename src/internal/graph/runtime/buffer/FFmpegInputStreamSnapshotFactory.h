#pragma once

#include "internal/graph/runtime/buffer/FFmpegInputStreamSnapshot.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <vector>

namespace media::ffmpeg::graph {

class FFmpegInputStreamSnapshotFactory final {
public:
    static ::media::Result<std::vector<FFmpegInputStreamSnapshot>> fromFormatContext(
        const AVFormatContext& context);
};

} // namespace media::ffmpeg::graph
