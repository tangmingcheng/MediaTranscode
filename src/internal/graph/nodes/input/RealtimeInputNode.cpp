#include "internal/graph/nodes/input/RealtimeInputNode.h"

namespace media::ffmpeg::graph {
RealtimeInputNode::RealtimeInputNode(MediaNodeId nodeId, MediaPreparedRealtimeInput prepared)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RealtimeInputNode")
    , m_formatBuffer(prepared.releaseFormatBuffer())
{
}

MediaNodeKind RealtimeInputNode::staticKind() noexcept
{
    return MediaNodeKind::RealtimeInput;
}

::media::Result<MediaNodeProcessResult> RealtimeInputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted) {
        return processFinished();
    }

    if (!m_formatBuffer) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::notInitialized("RealtimeInputNode requires prepared input binding"));
    }

    auto pushed = emitOutput(context, "format", m_formatBuffer);
    if (!pushed) {
        return processProgress(pushed);
    }

    m_formatEmitted = true;
    return processProgress();
}

::media::Status RealtimeInputNode::stop(MediaGraphExecutionContext& context)
{
    m_formatBuffer.reset();
    m_formatEmitted = false;
    return FFmpegNodeRuntime::stop(context);
}

void RealtimeInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_formatBuffer.reset();
    m_formatEmitted = false;
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
