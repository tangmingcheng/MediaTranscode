#include "internal/graph/nodes/output/RtpOutputNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <string>
#include <utility>

namespace media::ffmpeg::graph {

RtpOutputNode::RtpOutputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RtpOutputNode")
{
}

MediaNodeKind RtpOutputNode::staticKind() noexcept
{
    return MediaNodeKind::RtpOutput;
}

::media::Status RtpOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted) {
        return ::media::Status::success();
    }

    auto status = openIfNeeded(context);
    if (!status) {
        return status;
    }

    auto buffer = FFmpegBufferFactory::wrapOutputFormatContext(m_session.takeContext());
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    auto pushStatus = pushToAllOutputs(context, buffer.value());
    if (!pushStatus) {
        return pushStatus;
    }

    m_formatEmitted = true;
    return ::media::Status::success();
}

::media::Status RtpOutputNode::stop(MediaGraphExecutionContext& context)
{
    m_session.close();
    m_formatEmitted = false;
    return FFmpegNodeRuntime::stop(context);
}

void RtpOutputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_session.interrupt();
    m_session.close();
    m_formatEmitted = false;
    FFmpegNodeRuntime::abort(context);
}

::media::Status RtpOutputNode::openIfNeeded(MediaGraphExecutionContext& context)
{
    if (m_session.context()) {
        return ::media::Status::success();
    }

    FFmpegRtpOutputSessionOptions options;
    options.url = nodeOption(context, "url");
    const std::string timeout = nodeOption(context, "rtp.write_timeout_ms");
    if (!timeout.empty()) {
        options.writeTimeoutMs = std::stoi(timeout);
    }
    return m_session.open(std::move(options));
}

} // namespace media::ffmpeg::graph
