#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "media_transcode/Result.h"

#include <string>

namespace media::ffmpeg::graph {

struct FileInputSegmentOptions {
    std::string prefix = "input.file";
    std::string inputUrl;
};

struct FileInputSegment {
    MediaNodeId input = MediaNodeId::invalid();
    std::string formatPort = "format";
};

class MediaInputSegmentBuilder final {
public:
    static ::media::Result<FileInputSegment> buildFileInput(MediaGraph& graph,
                                                            const FileInputSegmentOptions& options);

private:
    MediaInputSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
