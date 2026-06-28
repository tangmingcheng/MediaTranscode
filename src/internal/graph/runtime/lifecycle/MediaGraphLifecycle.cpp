#include "internal/graph/runtime/lifecycle/MediaGraphLifecycle.h"

namespace media::ffmpeg::graph {

::media::Status MediaGraphLifecycle::closeChannels(MediaGraphExecutionContext& context)
{
    for (MediaChannel* channel : context.channels().channels()) {
        if (channel) {
            channel->close();
        }
    }

    return ::media::Status::success();
}

::media::Status MediaGraphLifecycle::clearChannels(MediaGraphExecutionContext& context)
{
    for (MediaChannel* channel : context.channels().channels()) {
        if (channel) {
            channel->clear();
        }
    }

    return ::media::Status::success();
}

void MediaGraphLifecycle::abortChannels(MediaGraphExecutionContext& context) noexcept
{
    for (MediaChannel* channel : context.channels().channels()) {
        if (channel) {
            channel->abort();
        }
    }
}

} // namespace media::ffmpeg::graph
