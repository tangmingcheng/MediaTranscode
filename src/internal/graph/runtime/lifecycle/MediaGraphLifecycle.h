#pragma once

#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
#include "media_transcode/Result.h"

namespace media::ffmpeg::graph {

class MediaGraphLifecycle final {
public:
    static ::media::Status closeChannels(MediaGraphExecutionContext& context);
    static ::media::Status clearChannels(MediaGraphExecutionContext& context);
    static void abortChannels(MediaGraphExecutionContext& context) noexcept;
};

} // namespace media::ffmpeg::graph
