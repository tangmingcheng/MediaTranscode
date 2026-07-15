#include "internal/graph/nodes/mux/FileMuxNode.h"

#include "internal/graph/core/MediaGraph.h"

#include <string>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {

FileMuxNode::FileMuxNode(MediaNodeId nodeId)
    : FileMuxNode(nodeId, std::make_unique<ExplicitMediaMuxSessionFactory>())
{
}

FileMuxNode::FileMuxNode(
    MediaNodeId nodeId,
    std::unique_ptr<MediaMuxSessionFactory> sessionFactory)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
    , m_sessionFactory(std::move(sessionFactory))
{
}

MediaNodeKind FileMuxNode::staticKind() noexcept
{
    return MediaNodeKind::FileMux;
}

::media::Result<MediaNodeProcessResult> FileMuxNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) return terminalResult();
    auto session = ensureSession(context);
    if (!session) return terminalResult();
    auto completion = bindCompletionInputs(context);
    if (!completion) return terminalResult();
    observeClosedInputs(context);

    auto ready = finishIfReady(context);
    if (!ready || ready.value().state == MediaNodeProcessState::Finished) return ready;

    auto input = tryPopFirstInputWithChannelOptional(context);
    if (!input) {
        remember(::media::Status::failure(input.error()));
        return terminalResult();
    }
    if (!input.value()) {
        observeClosedInputs(context);
        ready = finishIfReady(context);
        if (!ready || ready.value().state == MediaNodeProcessState::Finished) return ready;
        return pollOrWait(context);
    }

    MediaBufferRef buffer = input.value()->buffer;
    if (buffer->isEof()) {
        if (input.value()->channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
            m_completion->markEof(std::to_string(input.value()->channel->id().value));
        }
        return finishIfReady(context, buffer);
    }
    auto handled = handleBuffer(context, *input.value());
    if (!handled) return terminalResult();
    auto forwarded = remember(forwardIfOutputsExist(context, buffer));
    return forwarded ? processProgress() : terminalResult();
}

::media::Status FileMuxNode::flush(MediaGraphExecutionContext& context)
{
    auto session = ensureSession(context);
    if (!session) return session;
    auto flushed = remember(m_session->flush(context));
    return flushed ? FFmpegNodeRuntime::flush(context) : flushed;
}

::media::Status FileMuxNode::stop(MediaGraphExecutionContext& context)
{
    ::media::Status status = ensureSession(context);
    if (status) status = remember(m_session->finish(context));
    if (status) status = FFmpegNodeRuntime::stop(context);
    releaseSession();
    return status;
}

void FileMuxNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (!m_abortForwarded && m_session) {
        m_session->abort();
        m_abortForwarded = true;
    }
    FFmpegNodeRuntime::abort(context);
}

::media::Status FileMuxNode::ensureSession(MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) {
        return ::media::Status::failure(*m_terminalFailure);
    }
    if (m_session) return ::media::Status::success();
    if (!m_sessionFactory) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode requires a mux session factory")));
    }
    const MediaGraph* graph = context.graph();
    const MediaNode* node = graph ? graph->findNode(nodeId()) : nullptr;
    if (!node) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode requires compiled node options")));
    }
    auto created = m_sessionFactory->create(node->options);
    if (!created) {
        return remember(::media::Status::failure(created.error()));
    }
    m_session = std::move(created).value();
    if (!m_session) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::allocationFailed(
                "FileMuxNode session factory returned null")));
    }
    return ::media::Status::success();
}

::media::Status FileMuxNode::bindCompletionInputs(MediaGraphExecutionContext& context)
{
    if (m_completion) return ::media::Status::success();
    std::vector<std::string> inputs;
    bool hasEncodedPacketInput = false;
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (channel) {
            inputs.push_back(std::to_string(channel->id().value));
            hasEncodedPacketInput = hasEncodedPacketInput ||
                channel->binding().edgeKind == MediaEdgeKind::EncodedPacket;
        }
    }
    if (!hasEncodedPacketInput) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode requires an encoded packet input")));
    }
    m_completion = std::make_unique<MediaInputTerminalTracker>(std::move(inputs));
    return ::media::Status::success();
}

void FileMuxNode::observeClosedInputs(MediaGraphExecutionContext& context)
{
    if (!m_completion) return;
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (channel && channel->closed() && channel->size() == 0) {
            m_completion->markClosed(std::to_string(channel->id().value));
        }
    }
}

::media::Status FileMuxNode::handleBuffer(
    MediaGraphExecutionContext& context,
    const PoppedChannelBuffer& input)
{
    const MediaGraph* graph = context.graph();
    const MediaPort* port = graph
        ? graph->findPort(input.channel->binding().to.portId)
        : nullptr;
    if (!port) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode input port is unavailable")));
    }
    if (input.buffer->isFlush()) {
        return remember(m_session->flush(context));
    }
    if (port->name == "resource" || port->name == "plan") {
        return remember(m_session->bindResource(context, input.buffer));
    }
    if (port->name == "codec") {
        return remember(m_session->bindStreamConfig(context, input.buffer));
    }
    if (port->name == "packet") {
        return remember(m_session->write(context, input.buffer));
    }
    return remember(::media::Status::failure(
        ::media::ErrorInfo::invalidArgument(
            "FileMuxNode received input on unknown port")));
}

::media::Result<MediaNodeProcessResult> FileMuxNode::finishIfReady(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& terminalBuffer)
{
    if (!m_completion || !m_completion->finished()) {
        if (terminalBuffer) {
            auto forwarded = remember(forwardIfOutputsExist(context, terminalBuffer));
            if (!forwarded) return terminalResult();
        }
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::progress());
    }
    auto finished = remember(m_session->finish(context));
    if (!finished) return terminalResult();
    if (terminalBuffer) {
        auto forwarded = remember(forwardIfOutputsExist(context, terminalBuffer));
        if (!forwarded) return terminalResult();
    }
    return processFinished();
}

::media::Result<MediaNodeProcessResult> FileMuxNode::pollOrWait(
    MediaGraphExecutionContext& context)
{
    auto polled = m_session->poll(context);
    if (!polled) {
        remember(::media::Status::failure(polled.error()));
        return terminalResult();
    }
    if (polled.value().progressed) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::progress());
    }
    if (polled.value().nextWait) {
        const auto& wait = *polled.value().nextWait;
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::waitingUntil(
                wait.syncGroup, wait.masterDeadline));
    }
    return processWaiting();
}

::media::Status FileMuxNode::remember(::media::Status status)
{
    if (!status && !m_terminalFailure) m_terminalFailure = status.error();
    return m_terminalFailure
        ? ::media::Status::failure(*m_terminalFailure)
        : status;
}

::media::Result<MediaNodeProcessResult> FileMuxNode::terminalResult() const
{
    return ::media::Result<MediaNodeProcessResult>::failure(*m_terminalFailure);
}

::media::Status FileMuxNode::forwardIfOutputsExist(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& buffer)
{
    if (outputChannels(context).empty()) return ::media::Status::success();
    if (buffer->isEof() || buffer->isFlush()) {
        return broadcastControlToAllOutputs(context, buffer);
    }
    return pushToAllOutputs(context, buffer);
}

void FileMuxNode::releaseSession() noexcept
{
    m_session.reset();
    m_completion.reset();
    m_terminalFailure.reset();
    m_abortForwarded = false;
}

} // namespace media::ffmpeg::graph
