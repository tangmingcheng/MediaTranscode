#pragma once
#include "internal/graph/core/MediaNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
namespace media::ffmpeg::graph {
class MediaVideoFrameReadinessBinding final {
public:
    static ::media::Result<MediaVideoProducerFailureScope> guard(const MediaNodeOptions* options);
    static ::media::Result<MediaBufferRef> publish(::media::ffmpeg::FramePtr frame,
        const MediaNodeOptions* options, const std::shared_ptr<MediaPreparationMemoryResource>& resource,
        std::shared_ptr<const MediaFfmpegPayloadReferences> consumedInputs);
    static ::media::Status admitInput(const MediaBufferRef& buffer, const MediaNodeOptions* options);
};
}
