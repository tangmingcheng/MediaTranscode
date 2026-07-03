#pragma once

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/planner/MediaAudioPipelinePlanner.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaAudioEncodeOptionApplier final {
public:
    static ::media::Result<void> applyCodecResolverOptions(MediaGraph& graph,
                                                           MediaNodeId codecResolver,
                                                           const MediaAudioTranscodeParameters& audio,
                                                           const MediaAudioPipelinePlan& plan);

private:
    MediaAudioEncodeOptionApplier() = default;
};

} // namespace media::ffmpeg::graph
