#include "internal/graph/nodes/input/RealtimeInputNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {

RealtimeInputNode::RealtimeInputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RealtimeInputNode")
{
}

MediaNodeKind RealtimeInputNode::staticKind() noexcept
{
    return MediaNodeKind::RealtimeInput;
}

::media::Status RealtimeInputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted) {
        return ::media::Status::success();
    }

    auto status = openIfNeeded(context);
    if (!status) {
        return status;
    }

    auto buffer = FFmpegBufferFactory::wrapInputFormatContext(m_session.takeContext());
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    auto pushStatus = emitOutput(context, "format", buffer.value());
    if (!pushStatus) {
        return pushStatus;
    }

    m_formatEmitted = true;
    return ::media::Status::success();
}

::media::Status RealtimeInputNode::stop(MediaGraphExecutionContext& context)
{
    m_session.close();
    m_formatEmitted = false;
    return FFmpegNodeRuntime::stop(context);
}

void RealtimeInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_session.interrupt();
    m_session.close();
    m_formatEmitted = false;
    FFmpegNodeRuntime::abort(context);
}

::media::Status RealtimeInputNode::openIfNeeded(MediaGraphExecutionContext& context)
{
    if (m_session.context()) {
        return ::media::Status::success();
    }

    FFmpegRealtimeInputSessionOptions options;
    options.url = nodeOption(context, "url");
    options.sdpText = nodeOption(context, "input.sdp_text");
    options.sdpPath = nodeOption(context, "input.sdp_path");
    const std::string timeout = nodeOption(context, "input.read_timeout_ms");
    if (!timeout.empty()) {
        options.readTimeoutMs = std::stoi(timeout);
    }
    return m_session.open(std::move(options));
}

} // namespace media::ffmpeg::graph
