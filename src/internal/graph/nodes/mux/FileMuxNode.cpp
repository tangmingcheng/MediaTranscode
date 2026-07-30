#include "internal/graph/nodes/mux/FileMuxNode.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/sync/MediaProtocolOutputGenerationState.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

bool isBindingPort(std::string_view name) noexcept
{
    return name == "plan" || name == "resource" || name == "codec";
}

} // namespace

FileMuxNode::FileMuxNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
    , m_sessionFactory(std::make_unique<ExplicitMediaMuxSessionFactory>())
{
}

FileMuxNode::FileMuxNode(
    MediaNodeId nodeId,
    std::shared_ptr<MediaProtocolOutputGenerationState> generationState,
    std::shared_ptr<MediaUdpDatagramSenderPortFactory>
        datagramPortFactory)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
    , m_generationState(std::move(generationState))
    , m_sessionFactory(std::make_unique<ExplicitMediaMuxSessionFactory>(
          m_generationState, std::move(datagramPortFactory)))
{
}

FileMuxNode::FileMuxNode(
    MediaNodeId nodeId,
    std::unique_ptr<MediaMuxSessionFactory> sessionFactory)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
    , m_sessionFactory(std::move(sessionFactory))
{
}

std::shared_ptr<MediaAvGenerationPurgeTarget>
FileMuxNode::generationPurgeTarget() const noexcept
{
    return m_generationState;
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
    auto completion = bindInputTracking(context);
    if (!completion) return terminalResult();
    if (m_phase == Phase::AcquiringBindings) {
        auto bindings = validateAcquiringBindingChannels(context);
        if (!bindings) return terminalResult();
    }
    observeClosedInputs(context);

    auto ready = finishIfReady(context);
    if (!ready || ready.value().state == MediaNodeProcessState::Finished) return ready;

    static constexpr std::array<std::string_view, 3> bindingPorts{
        "plan", "resource", "codec"};
    auto input = m_phase == Phase::AcquiringBindings
        ? tryPopFirstInputWithChannelOptional(context, bindingPorts)
        : tryPopFirstInputWithChannelOptional(context);
    if (!input) {
        remember(::media::Status::failure(input.error()));
        return terminalResult();
    }
    if (!input.value()) {
        observeClosedInputs(context);
        ready = finishIfReady(context);
        if (!ready || ready.value().state == MediaNodeProcessState::Finished) return ready;
        return m_phase == Phase::AcquiringBindings
            ? processWaiting()
            : pollOrWait(context);
    }

    MediaBufferRef buffer = input.value()->buffer;
    if (buffer->isEof() &&
        isUnsatisfiedBindingChannel(*input.value()->channel)) {
        const auto& state = m_bindingInputs.at(
            input.value()->channel->id().value);
        remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode required " + state.portName +
                " binding channel reached EOF before a binding payload was accepted")));
        return terminalResult();
    }
    if (buffer->isEof()) {
        if (input.value()->channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
            m_completion->markEof(std::to_string(input.value()->channel->id().value));
        }
        return finishIfReady(context, buffer);
    }
    auto handled = handleBuffer(context, *input.value());
    if (!handled) return terminalResult();
    if (m_phase == Phase::AcquiringBindings &&
        allBindingChannelsSatisfied() && m_session->bindingsReady()) {
        m_phase = Phase::Streaming;
    }
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

::media::Status FileMuxNode::bindInputTracking(
    MediaGraphExecutionContext& context)
{
    if (m_completion) return ::media::Status::success();
    const MediaGraph* graph = context.graph();
    if (!graph) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode input tracking requires its compiled graph")));
    }
    std::vector<std::string> inputs;
    std::unordered_map<std::uint32_t, BindingInputState> bindingInputs;
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (channel) {
            if (channel->binding().edgeKind == MediaEdgeKind::EncodedPacket) {
                inputs.push_back(std::to_string(channel->id().value));
            }
            const MediaPort* port = graph->findPort(
                channel->binding().to.portId);
            if (!port) {
                return remember(::media::Status::failure(
                    ::media::ErrorInfo::notInitialized(
                        "FileMuxNode input tracking cannot resolve a target port")));
            }
            if (isBindingPort(port->name)) {
                bindingInputs.emplace(
                    channel->id().value,
                    BindingInputState{port->name, false});
            }
        }
    }
    if (inputs.empty()) {
        return remember(::media::Status::failure(
            ::media::ErrorInfo::notInitialized(
                "FileMuxNode requires an encoded packet input")));
    }
    m_completion = std::make_unique<MediaInputTerminalTracker>(std::move(inputs));
    m_bindingInputs = std::move(bindingInputs);
    return ::media::Status::success();
}

::media::Status FileMuxNode::validateAcquiringBindingChannels(
    MediaGraphExecutionContext& context)
{
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (!channel) continue;
        const auto found = m_bindingInputs.find(channel->id().value);
        if (found == m_bindingInputs.end() || found->second.satisfied) continue;
        if (channel->aborted()) {
            return remember(::media::Status::failure(
                ::media::ErrorInfo::cancelled(
                    "FileMuxNode required " + found->second.portName +
                    " binding channel aborted before a binding payload was accepted")));
        }
        if (channel->closed() && channel->size() == 0) {
            return remember(::media::Status::failure(
                ::media::ErrorInfo::notInitialized(
                    "FileMuxNode required " + found->second.portName +
                    " binding channel closed before a binding payload was accepted")));
        }
    }
    return ::media::Status::success();
}

bool FileMuxNode::isUnsatisfiedBindingChannel(
    const MediaChannel& channel) const noexcept
{
    const auto found = m_bindingInputs.find(channel.id().value);
    return found != m_bindingInputs.end() && !found->second.satisfied;
}

bool FileMuxNode::allBindingChannelsSatisfied() const noexcept
{
    return std::all_of(
        m_bindingInputs.begin(), m_bindingInputs.end(),
        [](const auto& entry) { return entry.second.satisfied; });
}

void FileMuxNode::observeClosedInputs(MediaGraphExecutionContext& context)
{
    if (!m_completion) return;
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (channel &&
            channel->binding().edgeKind == MediaEdgeKind::EncodedPacket &&
            channel->closed() && channel->size() == 0) {
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
    auto status = [&]() -> ::media::Status {
        if (port->name == "resource" || port->name == "plan") {
            return m_session->bindResource(context, input.buffer);
        }
        if (port->name == "codec") {
            return m_session->bindStreamConfig(context, input.buffer);
        }
        if (port->name == "packet") {
            return m_session->write(context, input.buffer);
        }
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(
                "FileMuxNode received input on unknown port"));
    }();
    status = remember(std::move(status));
    if (status && isBindingPort(port->name)) {
        const auto binding = m_bindingInputs.find(input.channel->id().value);
        if (binding == m_bindingInputs.end()) {
            return remember(::media::Status::failure(
                ::media::ErrorInfo::internalError(
                    "FileMuxNode accepted an untracked binding channel")));
        }
        binding->second.satisfied = true;
    }
    return status;
}

::media::Result<MediaNodeProcessResult> FileMuxNode::finishIfReady(
    MediaGraphExecutionContext& context,
    const MediaBufferRef& terminalBuffer)
{
    if (m_phase != Phase::Streaming ||
        !m_completion || !m_completion->finished()) {
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
    m_bindingInputs.clear();
    m_terminalFailure.reset();
    m_phase = Phase::AcquiringBindings;
    m_abortForwarded = false;
}

} // namespace media::ffmpeg::graph
