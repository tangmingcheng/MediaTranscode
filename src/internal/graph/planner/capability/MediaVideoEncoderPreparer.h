#pragma once

#include "internal/graph/builder/codec/CodecResolverEncoderContextBuilder.h"
#include "internal/graph/planner/MediaPipelinePlanner.h"
#include "internal/graph/planner/capability/MediaVideoEncoderReadback.h"

namespace media::ffmpeg::graph {

struct MediaPreparedVideoEncoder final {
    ::media::ffmpeg::CodecContextPtr context;
    MediaPreparedEncoderEmissionEnvelope emission;
    MediaVideoEncoderReadback readback;
};

// Opens once and retains the exact context whose readback was admitted.
// The request consumes existing planner options; no DAG or runtime is required.
class MediaVideoEncoderPreparer final {
public:
    static ::media::Result<MediaPreparedVideoEncoder> prepare(
        const CodecResolverEncoderContextBuildRequest& request,
        const MediaPipelineStagePlan& encoder);
private:
    MediaVideoEncoderPreparer() = delete;
};

} // namespace media::ffmpeg::graph
