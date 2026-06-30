#pragma once

#include "internal/graph/builder/local/LocalFilePlannerOptionBridge.h"
#include "internal/graph/builder/local/LocalFileTranscodeGraphBuilder.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class LocalFileNodeOptionApplier final {
public:
    static ::media::Status applyUserVideoOptions(MediaGraph& graph,
                                                 const LocalFilePlannerNodeIds& nodes,
                                                 const LocalFileTranscodeOptions& options);

private:
    LocalFileNodeOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
