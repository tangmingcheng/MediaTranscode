#pragma once

#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "media_transcode/Result.h"

#include <optional>
#include <string>

namespace media::ffmpeg::graph {

struct MediaLocalFileOutputPlan final {
    std::string url;
    std::string format;
    std::optional<MediaOutputResourceKind> outputResourceKind;
    std::optional<MediaMuxSessionKind> muxSessionKind;
};

class MediaLocalFileOutputPlanner final {
public:
    static ::media::Result<MediaLocalFileOutputPlan> plan(
        std::string outputUrl,
        std::string outputFormat);

private:
    MediaLocalFileOutputPlanner() = delete;
};

} // namespace media::ffmpeg::graph
