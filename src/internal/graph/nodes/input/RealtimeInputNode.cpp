#include "internal/graph/nodes/input/RealtimeInputNode.h"

namespace media::ffmpeg::graph {
RealtimeInputNode::RealtimeInputNode(MediaNodeId nodeId, MediaPreparedRealtimeInputKind expectedKind,
                                     MediaPreparedRealtimeInput prepared)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RealtimeInputNode")
    , m_expectedKind(expectedKind)
    , m_prepared(std::move(prepared))
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
        if (!m_prepared.kind() || *m_prepared.kind() != m_expectedKind) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                ::media::ErrorInfo::invalidArgument("prepared realtime input kind does not match planner binding"));
        }
        auto released = m_prepared.releaseBuffer();
        if (!released) return ::media::Result<MediaNodeProcessResult>::failure(released.error());
        m_formatBuffer = std::move(released).value();
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
