#include "internal/graph/nodes/merge/PacketMergeNode.h"
#include "internal/graph/runtime/buffer/MediaBuffer.h"

#include <string>

namespace media::ffmpeg::graph {

PacketMergeNode::PacketMergeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketMergeNode")
{
}

MediaNodeKind PacketMergeNode::staticKind() noexcept
{
    return MediaNodeKind::PacketMerge;
}

::media::Status PacketMergeNode::start(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    return FFmpegNodeRuntime::start(context);
}

::media::Status PacketMergeNode::stop(MediaGraphExecutionContext& context)
{
    resetRuntimeState();
    return FFmpegNodeRuntime::stop(context);
}

void PacketMergeNode::abort(MediaGraphExecutionContext& context) noexcept
{
    resetRuntimeState();
    FFmpegNodeRuntime::abort(context);
}

::media::Result<MediaNodeProcessResult> PacketMergeNode::onProcess(MediaGraphExecutionContext& context)
{
    bindInputs(context);
    auto input = tryPopFirstInputWithChannelOptional(context);
    if (!input) return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    if (!input.value()) {
        for (MediaChannel* channel : context.inputChannels(nodeId())) {
            if (channel && channel->closed()) m_terminals->markClosed(std::to_string(channel->id().value));
        }
        if (!m_terminals->finished()) return processWaiting();
        return m_terminalBuffer ? processFinished(pushToAllOutputs(context, m_terminalBuffer)) : processFinished();
    }

    auto& popped = *input.value();
    if (!popped.buffer->isEof()) return processProgress(pushToAllOutputs(context, popped.buffer));

    m_terminals->markEof(std::to_string(popped.channel->id().value));
    if (!m_terminalBuffer) m_terminalBuffer = popped.buffer;
    if (!m_terminals->finished()) return processProgress();
    return processFinished(pushToAllOutputs(context, m_terminalBuffer));
}

void PacketMergeNode::bindInputs(MediaGraphExecutionContext& context)
{
    if (m_terminals) return;
    std::vector<std::string> inputs;
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (channel) inputs.push_back(std::to_string(channel->id().value));
    }
    m_terminals = std::make_unique<MediaInputTerminalTracker>(std::move(inputs));
}

void PacketMergeNode::resetRuntimeState() noexcept
{
    m_terminals.reset();
    m_terminalBuffer.reset();
}

} // namespace media::ffmpeg::graph
