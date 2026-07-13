#pragma once

#include "media_transcode/Result.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <vector>

namespace media::ffmpeg::graph {

struct FFmpegInputProgramSnapshot final {
    int programNumber = 0;
    int pmtPid = 0;
    int pcrPid = 0;
    std::vector<int> streamIndexes;

    bool operator==(const FFmpegInputProgramSnapshot&) const = default;
};

class MediaTsPublicProgramSnapshotFactory final {
public:
    static ::media::Result<std::vector<FFmpegInputProgramSnapshot>> fromFormatContext(
        const AVFormatContext& context);
};

} // namespace media::ffmpeg::graph
