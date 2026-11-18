#include "internal/graph/nodes/input/MediaRtpControlOutputNode.h"
#include "internal/graph/runtime/context/MediaGraphExecutionContext.h"
namespace media::ffmpeg::graph {
MediaRtpControlOutputNode::MediaRtpControlOutputNode(MediaNodeId node)
    : FFmpegNodeRuntime(node, staticKind(), "MediaRtpControlOutputNode") {}
MediaNodeKind MediaRtpControlOutputNode::staticKind() noexcept { return MediaNodeKind::RtpControlOutput; }
::media::Status MediaRtpControlOutputNode::bindEndpoint(MediaPreparedRtpControlEndpoint endpoint)
{
    if (m_endpoint || m_started) return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
        "RTP control output requires exactly one endpoint before start"));
    m_endpoint.emplace(std::move(endpoint));
    return ::media::Status::success();
}
::media::Status MediaRtpControlOutputNode::start(MediaGraphExecutionContext& context)
{
    if (!m_endpoint || m_started) return ::media::Status::failure(::media::ErrorInfo::notInitialized(
        "RTP control output has no unstarted endpoint"));
    if (auto status = FFmpegNodeRuntime::start(context); !status) return status;
    if (auto status = m_endpoint->activate(context.sharedNodeWakeup(nodeId())); !status) return status;
    m_started = true;
    return ::media::Status::success();
}
::media::Result<MediaNodeProcessResult> MediaRtpControlOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (!m_started || !m_endpoint) return processProgress(::media::Status::failure(
        ::media::ErrorInfo::notInitialized("RTP control output is not started")));
    // FFmpegNodeRuntime flushes its pending transfer before invoking onProcess.
    auto output = m_endpoint->take();
    if (!output) return processProgress(::media::Status::failure(output.error()));
    if (!output.value()) return processWaiting();
    if (output.value()->port != MediaPreparedRtpOutputPort::Clock &&
        output.value()->port != MediaPreparedRtpOutputPort::Event)
        return processProgress(::media::Status::failure(::media::ErrorInfo::internalError(
            "RTP control endpoint returned media")));
    return processProgress(emitOutput(context, mediaPreparedRtpOutputPortName(output.value()->port), output.value()->buffer));
}
::media::Status MediaRtpControlOutputNode::stop(MediaGraphExecutionContext& context)
{
    if (m_endpoint) m_endpoint->deactivate();
    m_started = false;
    return FFmpegNodeRuntime::stop(context);
}
void MediaRtpControlOutputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (m_endpoint) m_endpoint->deactivate();
    m_started = false;
    FFmpegNodeRuntime::abort(context);
}
} // namespace media::ffmpeg::graph
