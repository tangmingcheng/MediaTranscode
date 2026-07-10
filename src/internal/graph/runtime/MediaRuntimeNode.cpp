#include "internal/graph/runtime/MediaRuntimeNode.h"

namespace media::ffmpeg::graph {

::media::Status MediaRuntimeNode::configure(MediaGraphExecutionContext&)
{
    return ::media::Status::success();
}

::media::Status MediaRuntimeNode::start(MediaGraphExecutionContext&)
{
    return ::media::Status::success();
}

::media::Status MediaRuntimeNode::flush(MediaGraphExecutionContext&)
{
    return ::media::Status::success();
}

::media::Status MediaRuntimeNode::stop(MediaGraphExecutionContext&)
{
    return ::media::Status::success();
}

void MediaRuntimeNode::abort(MediaGraphExecutionContext&) noexcept
{
}

void MediaRuntimeNode::interrupt(MediaGraphExecutionContext&) noexcept
{
}

} // namespace media::ffmpeg::graph
