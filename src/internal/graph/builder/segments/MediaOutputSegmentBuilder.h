#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "media_transcode/Result.h"

#include <string>
#include <optional>

namespace media::ffmpeg::graph {

struct FileOutputSegmentOptions {
    std::string prefix = "output.file";
    std::string outputUrl;
    std::string outputFormat;
    std::optional<MediaOutputResourceKind> outputResourceKind;
    bool expectVideo = false;
    bool expectAudio = false;
    std::optional<MediaMuxSessionKind> muxSessionKind;
    MediaGraphQueueParameters queues;
};

struct FileOutputSegment {
    MediaNodeId fileOutput = MediaNodeId::invalid();
    MediaNodeId mux = MediaNodeId::invalid();
};

class MediaOutputSegmentBuilder final {
public:
    static ::media::Result<FileOutputSegment> buildFileMuxOutput(MediaGraph& graph,
                                                                 const FileOutputSegmentOptions& options);

private:
    MediaOutputSegmentBuilder() = default;
};

} // namespace media::ffmpeg::graph
