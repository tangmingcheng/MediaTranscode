#include "internal/graph/nodes/packet/PacketStartGateNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* RequireKeyFrameOption = "packet_start_gate.require_key_frame";

} // namespace

PacketStartGateNode::PacketStartGateNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketStartGateNode")
{
}

MediaNodeKind PacketStartGateNode::staticKind() noexcept
{
    return MediaNodeKind::PacketStartGate;
}

::media::Result<MediaNodeProcessResult> PacketStartGateNode::onProcess(MediaGraphExecutionContext& context)
{
    auto configured = configure(context);
    if (!configured) {
        return processProgress(configured);
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return processWaiting();
    }

    MediaBufferRef buffer = *input.value();
    if (buffer->isEof() || buffer->isFlush()) {
        auto status = emitOutput(context, "packet", buffer);
        return buffer->isEof() ? processFinished(status) : processProgress(status);
    }

    if (!m_open && m_requireKeyFrame && !buffer->isKeyFrame()) {
        return processProgress();
    }

    if (!m_open) {
        m_open = true;
        mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::State,
                                MediaGraphDiagnosticPhase::RuntimeNode,
                                "packet_start_gate.open");
    }

    return processProgress(emitOutput(context, "packet", buffer));
}

::media::Status PacketStartGateNode::stop(MediaGraphExecutionContext& context)
{
    reset();
    return FFmpegNodeRuntime::stop(context);
}

void PacketStartGateNode::abort(MediaGraphExecutionContext& context) noexcept
{
    reset();
    FFmpegNodeRuntime::abort(context);
}

::media::Status PacketStartGateNode::configure(MediaGraphExecutionContext& context)
{
    if (m_configured) {
        return ::media::Status::success();
    }

    auto requireKeyFrame = requiredBoolNodeOption(nodeOptions(context),
                                                 "PacketStartGateNode",
                                                 RequireKeyFrameOption);
    if (!requireKeyFrame) {
        return ::media::Status::failure(requireKeyFrame.error());
    }
    m_requireKeyFrame = requireKeyFrame.value();
    m_open = !m_requireKeyFrame;
    m_configured = true;
    return ::media::Status::success();
}

void PacketStartGateNode::reset() noexcept
{
    m_configured = false;
    m_requireKeyFrame = false;
    m_open = false;
}

} // namespace media::ffmpeg::graph
