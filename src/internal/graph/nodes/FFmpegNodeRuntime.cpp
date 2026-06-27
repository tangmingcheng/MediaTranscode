#include "internal/graph/nodes/FFmpegNodeRuntime.h"

#include "internal/graph/runtime/channel/MediaChannel.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegNodeRuntime::FFmpegNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name)
    : MediaNodeRuntime(nodeId, kind, std::move(name))
{
}

::media::Result<MediaBufferRef> FFmpegNodeRuntime::popInput(MediaGraphExecutionContext& context,
                                                             const std::string& portName)
{
    MediaChannel* channel = context.findInputChannel(nodeId(), portName);
    if (!channel) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime popInput failed: input channel not found"));
    }

    MediaBufferRef buffer;
    auto status = channel->pop(buffer);
    if (!status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }

    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Status FFmpegNodeRuntime::pushOutput(MediaGraphExecutionContext& context,
                                               const std::string& portName,
                                               MediaBufferRef buffer)
{
    MediaChannel* channel = context.findOutputChannel(nodeId(), portName);
    if (!channel) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime pushOutput failed: output channel not found"));
    }

    return channel->push(std::move(buffer));
}

::media::Status FFmpegNodeRuntime::forward(MediaGraphExecutionContext& context,
                                            const std::string& inputPortName,
                                            const std::string& outputPortName)
{
    auto buffer = popInput(context, inputPortName);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    return pushOutput(context, outputPortName, std::move(buffer).value());
}

} // namespace media::ffmpeg::graph
