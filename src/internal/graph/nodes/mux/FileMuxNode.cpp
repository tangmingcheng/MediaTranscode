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
    std::shared_ptr<MediaProtocolOutputRuntimeAuthority> outputAuthority)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileMuxNode")
    , m_generationState(std::move(generationState))
    , m_outputAuthority(outputAuthority)
    , m_generationPurge(std::make_shared<MediaOwnerThreadGenerationPurge>())
    , m_sessionFactory(std::make_unique<ExplicitMediaMuxSessionFactory>(
          m_generationState, std::move(outputAuthority)))
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
    return m_generationPurge;
}

MediaNodeKind FileMuxNode::staticKind() noexcept
{
    return MediaNodeKind::FileMux;
}

::media::Result<MediaNodeProcessResult> FileMuxNode::onProcess(
    MediaGraphExecutionContext& context)
{
    if (m_terminalFailure) return terminalResult();
    if (m_phase == Phase::AwaitingGenerationPurge) return processWaiting();
    auto session = ensureSession(context);
    if (!session) return terminalResult();
    auto completion = bindInputTracking(context);
    if (!completion) return terminalResult();
    if (m_phase == Phase::AcquiringBindings) {
        auto bindings = validateAcquiringBindingChannels(context);
        if (!bindings) return terminalResult();
    }
    observeClosedInputs(context);

    if (m_phase == Phase::Streaming && m_session->hasPendingOutput()) {
        return pollOrWait(context);
    }

    if (m_pendingSessionInput) {
        auto retained = m_pendingSessionInput;
        if (m_pendingFlushForward) {
            m_pendingSessionInput.reset();
            m_pendingFlushForward = false;
            return processProgress(forwardIfOutputsExist(context, retained));
        }
        auto written = submitSessionInput(context, retained);
        if (!written && written.error().code == ::media::ErrorCode::WouldBlock &&
            m_session->hasPendingOutput()) return pollOrWait(context);
        if (written && retained->isFlush()) {
            if (m_session->hasPendingOutput()) {
                m_pendingSessionInput = retained;
                m_pendingFlushForward = true;
                return pollOrWait(context);
            }
            return processProgress(forwardIfOutputsExist(context, retained));
        }
        return processProgress(std::move(written));
    }

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
    if (!handled) {
        if (handled.error().code == ::media::ErrorCode::WouldBlock && m_pendingSessionInput) {
            if (m_session->hasPendingOutput()) return pollOrWait(context);
            return processProgress(std::move(handled));
        }
        return terminalResult();
    }
    if (m_phase == Phase::AcquiringBindings &&
        allBindingChannelsSatisfied() && m_session->bindingsReady()) {
        m_phase = Phase::Streaming;
    }
    if (m_phase == Phase::Streaming && m_session->hasPendingOutput()) {
        if (buffer->isFlush()) {
            m_pendingSessionInput = buffer;
            m_pendingFlushForward = true;
        }
        return pollOrWait(context);
    }
    return processProgress(forwardIfOutputsExist(context, buffer));
}

::media::Status FileMuxNode::flush(MediaGraphExecutionContext& context)
{
    auto session = ensureSession(context);
    if (!session) return session;
    auto flushed = m_session->flush(context);
    if (!flushed && m_generationPurge &&
        flushed.error().code == ::media::ErrorCode::WouldBlock) return flushed;
    flushed = remember(std::move(flushed));
    return flushed ? FFmpegNodeRuntime::flush(context) : flushed;
}

::media::Status FileMuxNode::stop(MediaGraphExecutionContext& context)
{
    if (m_generationPurge) m_generationPurge->stop();
    ::media::Status status = m_terminalFailure
        ? ::media::Status::failure(*m_terminalFailure) : ::media::Status::success();
    // Stopping never starts a mux or turns incomplete preparation into EOF.
    if (status && m_session)
        status = remember(m_session->finish(context, MediaMuxCompletionIntent::RequestedStop));
    if (status) status = FFmpegNodeRuntime::stop(context);
    releaseSession();
    return status;
}

void FileMuxNode::abort(MediaGraphExecutionContext& context) noexcept
{
    if (m_generationPurge) m_generationPurge->stop();
    if (!m_abortForwarded && m_session) {
        m_session->abort();
        m_abortForwarded = true;
    }
    FFmpegNodeRuntime::abort(context);
    releaseSession();
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
    auto created = m_sessionFactory->create(node->options, context.findNodeWakeup(nodeId()));
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
    if (input.buffer->isFlush() || port->name == "packet")
        return submitSessionInput(context, input.buffer);
    auto status = [&]() -> ::media::Status {
        if (port->name == "resource" || port->name == "plan") {
            return m_session->bindResource(context, input.buffer);
        }
        if (port->name == "codec") {
            return m_session->bindStreamConfig(context, input.buffer);
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
        return terminalBuffer
            ? processProgress(forwardIfOutputsExist(context, terminalBuffer))
            : processProgress();
    }
    auto finished = m_session->finish(context, MediaMuxCompletionIntent::EndOfInput);
    if (!finished && m_generationPurge && finished.error().code == ::media::ErrorCode::WouldBlock) {
        // The protocol adapter emits WouldBlock only for a revoked finish
        // permit. Keep EOF tracking until the owner-thread purge resets it;
        // do not finish the worker or publish this retiring generation's EOF.
        m_phase = Phase::AwaitingGenerationPurge;
        return processWaiting();
    }
    finished = remember(std::move(finished));
    if (!finished) return terminalResult();
    // The base owns a blocked terminal transfer and must remember completion
    // while it retries that transfer. WouldBlock is never a mux failure.
    return terminalBuffer
        ? processFinished(forwardIfOutputsExist(context, terminalBuffer))
        : processFinished();
}

::media::Result<MediaNodeProcessResult> FileMuxNode::pollOrWait(
    MediaGraphExecutionContext& context)
{
    auto polled = m_session->poll(context);
    if (!polled) {
        if (m_generationPurge && polled.error().code == ::media::ErrorCode::WouldBlock)
            return processWaiting();
        remember(::media::Status::failure(polled.error()));
        return terminalResult();
    }
    if (polled.value().output) {
        auto emitted = pushToAllOutputs(context, polled.value().output);
        if (!emitted &&
            emitted.error().code == ::media::ErrorCode::WouldBlock) {
            return ::media::Result<MediaNodeProcessResult>::failure(
                emitted.error());
        }
        emitted = remember(std::move(emitted));
        return emitted ? processProgress() : terminalResult();
    }
    if (polled.value().progressed) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::progress());
    }
    if (polled.value().nextWait) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult{
                MediaNodeProcessState::Waiting,
                polled.value().nextWait});
    }
    return processWaiting();
}

::media::Status FileMuxNode::submitSessionInput(
    MediaGraphExecutionContext& context, const MediaBufferRef& buffer)
{
    auto status = buffer->isFlush()
        ? m_session->flush(context)
        : m_session->write(context, buffer);
    if (!status && m_generationPurge && status.error().code == ::media::ErrorCode::WouldBlock) {
        // A pressured packet is unaccepted; a pressured flush still needs advancement.
        // Retain one input and drain protocol output before retrying either operation.
        m_pendingSessionInput = buffer;
        return status;
    }
    m_pendingSessionInput.reset();
    return remember(std::move(status));
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
    auto forwarded = [&]() -> ::media::Status {
        if (outputChannels(context).empty()) return ::media::Status::success();
        if (buffer->isEof() || buffer->isFlush()) {
            return broadcastControlToAllOutputs(context, buffer);
        }
        if (buffer->payloadKind() == MediaPayloadKind::MpegTsProtocolDatagramBatch) {
            return pushToAllOutputs(context, buffer);
        }
        return ::media::Status::success();
    }();
    // Pending publication is owned by FFmpegNodeRuntime; caching this status
    // as m_terminalFailure would strand the mux after the pending EOF is sent.
    if (!forwarded && forwarded.error().code == ::media::ErrorCode::WouldBlock)
        return forwarded;
    return remember(std::move(forwarded));
}

void FileMuxNode::releaseSession() noexcept
{
    m_pendingFlushForward = false;
    m_pendingSessionInput.reset();
    m_session.reset();
    m_completion.reset();
    m_bindingInputs.clear();
    m_terminalFailure.reset();
    m_phase = Phase::AcquiringBindings;
    m_abortForwarded = false;
}

} // namespace media::ffmpeg::graph
