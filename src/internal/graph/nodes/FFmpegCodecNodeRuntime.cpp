#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"

#include <utility>

namespace media::ffmpeg::graph {

FFmpegCodecNodeRuntime::FFmpegCodecNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name)
    : FFmpegNodeRuntime(nodeId, kind, std::move(name))
{
}

::media::Status FFmpegCodecNodeRuntime::start(MediaGraphExecutionContext& context)
{
    resetCodecContext();
    return FFmpegNodeRuntime::start(context);
}

::media::Status FFmpegCodecNodeRuntime::stop(MediaGraphExecutionContext& context)
{
    auto status = FFmpegNodeRuntime::stop(context);
    resetCodecContext();
    return status;
}

void FFmpegCodecNodeRuntime::abort(MediaGraphExecutionContext& context) noexcept
{
    FFmpegNodeRuntime::abort(context);
    resetCodecContext();
}

void FFmpegCodecNodeRuntime::resetCodecContext() noexcept
{
    m_codecContext = nullptr;
    m_codecContextOwner.reset();
}

bool FFmpegCodecNodeRuntime::tryBindCodecContext(const MediaBufferRef& buffer) noexcept
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    if (!codecBuffer || !codecBuffer->context()) {
        return false;
    }

    m_codecContextOwner = buffer;
    m_codecContext = codecBuffer->context();
    return true;
}

AVCodecContext* FFmpegCodecNodeRuntime::codecContext() noexcept
{
    return m_codecContext;
}

const AVCodecContext* FFmpegCodecNodeRuntime::codecContext() const noexcept
{
    return m_codecContext;
}

bool FFmpegCodecNodeRuntime::hasCodecContext() const noexcept
{
    return m_codecContext != nullptr;
}

} // namespace media::ffmpeg::graph
