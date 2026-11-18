#pragma once

#include "internal/graph/runtime/ffmpeg/FFmpegRAII.h"
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/model/MediaFormatDescriptor.h"
#include "internal/graph/model/MediaVideoExecutionContract.h"
#include "internal/graph/planner/capability/MediaDecoderRuntimeFacts.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

struct CodecResolverDecoderContextBuildRequest {
    const AVCodecParameters* codecParameters;
    MediaTimeDescriptor sourceTime;
    const MediaNodeOptions* options;
    AVBufferRef* hardwareDevice;
    // Preparation has no runtime lineage registry capacity yet, but the
    // retained codec must open with the planner's exact production propagation.
    std::optional<MediaVideoLineagePropagation> preparationLineage;
};

struct CodecResolverDecoderContextBuildResult {
    ::media::ffmpeg::CodecContextPtr context;
    ::media::ffmpeg::BufferRefPtr hardwareDevice;
    MediaDecoderRuntimeFacts runtimeFacts;
};

class CodecResolverDecoderContextBuilder final {
public:
    static ::media::Result<CodecResolverDecoderContextBuildResult> build(
        const CodecResolverDecoderContextBuildRequest& request);
};

} // namespace media::ffmpeg::graph
