#include "internal/graph/nodes/FFmpegNodeRuntime.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/MediaControlBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {

::media::Status FFmpegNodeRuntime::start(MediaGraphExecutionContext& context)
{
    m_nextInputIndex = 0;
    m_pendingTransfer.reset();
    m_finishPending = false;
    m_finished = false;
    return MediaNodeRuntime::start(context);
}

::media::Status FFmpegNodeRuntime::stop(MediaGraphExecutionContext& context)
{
    m_nextInputIndex = 0;
    m_pendingTransfer.reset();
    m_finishPending = false;
    m_finished = false;
    return MediaNodeRuntime::stop(context);
}

void FFmpegNodeRuntime::abort(MediaGraphExecutionContext& context) noexcept
{
    m_nextInputIndex = 0;
    m_pendingTransfer.reset();
    m_finishPending = false;
    m_finished = false;
    MediaNodeRuntime::abort(context);
}

::media::Result<MediaNodeProcessResult> FFmpegNodeRuntime::process(MediaGraphExecutionContext& context)
{
    if (m_finished) {
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::finished());
    }
    if (m_pendingTransfer && !pendingOutputIsCurrent(m_pendingTransfer->buffer)) {
        cancelPendingOutputTransfer();
    }
    const bool hadPendingTransfer = m_pendingTransfer.has_value();
    bool waiting = false;
    auto pendingStatus = drainPendingTransfers(context, waiting);
    if (!pendingStatus) {
        return ::media::Result<MediaNodeProcessResult>::failure(pendingStatus.error());
    }
    if (waiting) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }
    if (hadPendingTransfer) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (m_finishPending) {
        for (MediaChannel* channel : context.outputChannels(nodeId())) {
            if (channel) {
                channel->close();
            }
        }
        m_finishPending = false;
        m_finished = true;
        return ::media::Result<MediaNodeProcessResult>::success(
            MediaNodeProcessResult::finished());
    }
    auto outcome = MediaNodeRuntime::process(context);
    if (!outcome && outcome.error().code == ::media::ErrorCode::WouldBlock) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }
    if (outcome && outcome.value().state == MediaNodeProcessState::Finished &&
        m_pendingTransfer.has_value()) {
        m_finishPending = true;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    if (outcome && outcome.value().state == MediaNodeProcessState::Finished) {
        m_finished = true;
    }
    return outcome;
}

bool FFmpegNodeRuntime::canFinishProcess() const noexcept
{
    return !m_pendingTransfer.has_value();
}

::media::Result<MediaNodeProcessResult> FFmpegNodeRuntime::processProgress(::media::Status status)
{
    if (!status && status.error().code == ::media::ErrorCode::WouldBlock) {
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::waiting());
    }
    return MediaNodeRuntime::processProgress(std::move(status));
}

::media::Result<MediaNodeProcessResult> FFmpegNodeRuntime::processFinished(::media::Status status)
{
    if (!status && status.error().code == ::media::ErrorCode::WouldBlock) {
        m_finishPending = true;
        return ::media::Result<MediaNodeProcessResult>::success(MediaNodeProcessResult::progress());
    }
    return MediaNodeRuntime::processFinished(std::move(status));
}

std::size_t FFmpegNodeRuntime::pendingOutputBufferCount() const noexcept
{
    return m_pendingTransfer ? 1u : 0u;
}

bool FFmpegNodeRuntime::retainsPendingOutput(const MediaBufferRef& buffer) const noexcept
{
    return m_pendingTransfer && m_pendingTransfer->buffer == buffer;
}

bool FFmpegNodeRuntime::pendingOutputIsCurrent(const MediaBufferRef&) const noexcept
{
    return true;
}

void FFmpegNodeRuntime::cancelPendingOutputTransfer() noexcept
{
    m_pendingTransfer.reset();
    m_finishPending = false;
    m_finished = false;
}
namespace {

bool isWildcardStream(MediaStreamKind kind) noexcept
{
    return kind == MediaStreamKind::Any || kind == MediaStreamKind::Unknown;
}

bool isWildcardPayload(MediaPayloadKind kind) noexcept
{
    return kind == MediaPayloadKind::Unknown;
}

bool isStreamCompatibleControlBuffer(
    const MediaChannel& channel,
    const MediaBufferRef& buffer) noexcept
{
    if (!buffer ||
        buffer->payloadKind() != MediaPayloadKind::ControlSignal) {
        return false;
    }
    const auto* control = dynamic_cast<const MediaControlBuffer*>(buffer.get());
    if (!control) {
        return false;
    }
    switch (control->controlKind()) {
    case MediaControlBufferKind::Eof:
    case MediaControlBufferKind::Flush:
    case MediaControlBufferKind::Abort:
        return true;
    case MediaControlBufferKind::Unknown:
        return false;
    }
    return false;
}

bool isControlBroadcastBuffer(const MediaBufferRef& buffer) noexcept
{
    if (!buffer || buffer->payloadKind() != MediaPayloadKind::ControlSignal) {
        return false;
    }
    const auto* control = dynamic_cast<const MediaControlBuffer*>(buffer.get());
    return control && control->controlKind() != MediaControlBufferKind::Unknown;
}

::media::Status validateChannelBufferType(const MediaChannel& channel,
                                          const MediaBufferRef& buffer,
                                          const char* action)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(std::string(action) + " failed: buffer is null"));
    }

    if (isStreamCompatibleControlBuffer(channel, buffer)) {
        return ::media::Status::success();
    }

    const auto& binding = channel.binding();
    if (!isWildcardStream(binding.streamKind) && binding.streamKind != buffer->streamKind()) {
        std::ostringstream out;
        out << action << " failed: stream type mismatch channel="
            << mediaGraphDiagnosticStreamKindName(binding.streamKind)
            << " buffer=" << mediaGraphDiagnosticStreamKindName(buffer->streamKind())
            << " " << mediaGraphDiagnosticDescribeChannel(channel)
            << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    if (!isWildcardPayload(binding.payloadKind) && binding.payloadKind != buffer->payloadKind()) {
        std::ostringstream out;
        out << action << " failed: payload type mismatch channel="
            << mediaGraphDiagnosticPayloadKindName(binding.payloadKind)
            << " buffer=" << mediaGraphDiagnosticPayloadKindName(buffer->payloadKind())
            << " " << mediaGraphDiagnosticDescribeChannel(channel)
            << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    const auto& channelFormat = channel.formatDescriptor();
    const auto& bufferFormat = buffer->formatDescriptor();
    if (channelFormat.hasStreamIndex() &&
        bufferFormat.hasStreamIndex() &&
        channelFormat.streamIndex != bufferFormat.streamIndex) {
        std::ostringstream out;
        out << action << " failed: stream index mismatch channel="
            << channelFormat.streamIndex
            << " buffer=" << bufferFormat.streamIndex
            << " " << mediaGraphDiagnosticDescribeChannel(channel)
            << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    return ::media::Status::success();
}

void logEdgeTransfer(MediaGraphExecutionContext& context,
                     MediaGraphDiagnosticPhase phase,
                     const char* action,
                     MediaNodeId nodeId,
                     const std::string& nodeName,
                     const MediaChannel& channel,
                     const MediaBufferRef& buffer)
{
    const std::string key = std::to_string(nodeId.value) + ":" + std::to_string(channel.edgeId().value) + ":" + action;

    auto decision = mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel::Flow, key);
    if (!decision.shouldLog) {
        return;
    }

    std::ostringstream out;
    out << action
        << " node=" << nodeId.value
        << " node_name=" << nodeName
        << " seq=" << decision.sequence
        << " " << mediaGraphDiagnosticDescribeChannel(channel)
        << " " << mediaGraphDiagnosticDescribeBuffer(buffer);

    mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel::Flow, phase, out.str());
}

} // namespace

FFmpegNodeRuntime::FFmpegNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name)
    : MediaNodeRuntime(nodeId, kind, std::move(name))
{
}

const MediaNodeOptions* FFmpegNodeRuntime::nodeOptions(MediaGraphExecutionContext& context) const noexcept
{
    const MediaGraph* graph = context.graph();
    if (!graph) {
        return nullptr;
    }

    const MediaNode* node = graph->findNode(nodeId());
    return node ? &node->options : nullptr;
}

std::string FFmpegNodeRuntime::nodeOption(MediaGraphExecutionContext& context,
                                           const std::string& key,
                                           std::string missingValue) const
{
    const MediaNodeOptions* options = nodeOptions(context);
    return options ? options->value(key, std::move(missingValue)) : std::move(missingValue);
}

::media::Result<MediaBufferRef> FFmpegNodeRuntime::popInput(MediaGraphExecutionContext& context,
                                                             const std::string& portName)
{
    MediaChannel* channel = context.findInputChannel(nodeId(), portName);
    if (!channel) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime popInput failed: input channel not found: " + portName));
    }

    MediaBufferRef buffer;
    auto status = channel->pop(buffer);
    if (!status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }

    auto typeStatus = validateChannelBufferType(*channel, buffer, "popInput");
    if (!typeStatus) {
        return ::media::Result<MediaBufferRef>::failure(typeStatus.error());
    }

    logEdgeTransfer(context,
                    MediaGraphDiagnosticPhase::RuntimeEdge,
                    "pop",
                    nodeId(),
                    name(),
                    *channel,
                    buffer);

    return ::media::Result<MediaBufferRef>::success(std::move(buffer));
}

::media::Result<MediaBufferRef> FFmpegNodeRuntime::tryPopFirstInput(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaBufferRef>::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime tryPopFirstInput failed: no input buffer available"));
    }
    return ::media::Result<MediaBufferRef>::success(std::move(*input.value()));
}

::media::Result<std::optional<MediaBufferRef>> FFmpegNodeRuntime::tryPopFirstInputOptional(MediaGraphExecutionContext& context)
{
    const auto& channels = context.inputChannels(nodeId());
    if (channels.empty()) return ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt);
    const std::size_t start = m_nextInputIndex % channels.size();
    for (std::size_t offset = 0; offset < channels.size(); ++offset) {
        const std::size_t index = (start + offset) % channels.size();
        MediaChannel* channel = channels[index];
        if (!channel) {
            continue;
        }

        MediaBufferRef buffer;
        if (channel->tryPop(buffer)) {
            auto typeStatus = validateChannelBufferType(*channel, buffer, "tryPopFirstInput");
            if (!typeStatus) {
                return ::media::Result<std::optional<MediaBufferRef>>::failure(typeStatus.error());
            }

            logEdgeTransfer(context,
                            MediaGraphDiagnosticPhase::RuntimeEdge,
                            "try_pop",
                            nodeId(),
                            name(),
                            *channel,
                            buffer);
            m_nextInputIndex = (index + 1) % channels.size();
            return ::media::Result<std::optional<MediaBufferRef>>::success(std::move(buffer));
        }
    }

    m_nextInputIndex = (start + 1) % channels.size();

    return ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt);
}

::media::Result<std::optional<FFmpegNodeRuntime::PoppedChannelBuffer>>
FFmpegNodeRuntime::tryPopFirstInputWithChannelOptional(MediaGraphExecutionContext& context)
{
    const auto& channels = context.inputChannels(nodeId());
    if (channels.empty()) return ::media::Result<std::optional<PoppedChannelBuffer>>::success(std::nullopt);
    const std::size_t start = m_nextInputIndex % channels.size();
    for (std::size_t offset = 0; offset < channels.size(); ++offset) {
        const std::size_t index = (start + offset) % channels.size();
        MediaChannel* channel = channels[index];
        if (!channel) continue;
        MediaBufferRef buffer;
        if (!channel->tryPop(buffer)) continue;
        auto typeStatus = validateChannelBufferType(*channel, buffer, "tryPopFirstInputWithChannel");
        if (!typeStatus) {
            return ::media::Result<std::optional<PoppedChannelBuffer>>::failure(typeStatus.error());
        }
        m_nextInputIndex = (index + 1) % channels.size();
        return ::media::Result<std::optional<PoppedChannelBuffer>>::success(
            PoppedChannelBuffer{ channel, std::move(buffer) });
    }
    m_nextInputIndex = (start + 1) % channels.size();
    return ::media::Result<std::optional<PoppedChannelBuffer>>::success(std::nullopt);
}

::media::Result<std::optional<MediaBufferRef>> FFmpegNodeRuntime::tryPopInputOptional(MediaGraphExecutionContext& context,
                                                                                       const std::string& portName)
{
    MediaChannel* channel = context.findInputChannel(nodeId(), portName);
    if (!channel) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime tryPopInputOptional failed: input channel not found: " + portName));
    }

    MediaBufferRef buffer;
    if (!channel->tryPop(buffer)) {
        return ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt);
    }

    auto typeStatus = validateChannelBufferType(*channel, buffer, "tryPopInputOptional");
    if (!typeStatus) {
        return ::media::Result<std::optional<MediaBufferRef>>::failure(typeStatus.error());
    }

    logEdgeTransfer(context,
                    MediaGraphDiagnosticPhase::RuntimeEdge,
                    "try_pop",
                    nodeId(),
                    name(),
                    *channel,
                    buffer);

    return ::media::Result<std::optional<MediaBufferRef>>::success(std::move(buffer));
}

::media::Status FFmpegNodeRuntime::emitOutput(MediaGraphExecutionContext& context,
                                               const std::string& portName,
                                               const MediaBufferRef& buffer)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegNodeRuntime emitOutput failed: buffer is null"));
    }

    const MediaGraph* graph = context.graph();
    const MediaPort* port = graph ? graph->findOutputPort(nodeId(), portName) : nullptr;
    if (!port) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime emitOutput failed: output port not found"));
    }

    std::vector<MediaChannel*> targets;
    for (MediaChannel* channel : context.outputChannels(nodeId())) {
        if (!channel || channel->binding().from.portId != port->id) {
            continue;
        }

        targets.push_back(channel);
    }

    if (targets.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime emitOutput failed: no output channel"));
    }

    return transferOrDefer(context, targets, buffer, "emit");
}

::media::Status FFmpegNodeRuntime::pushToAllOutputs(MediaGraphExecutionContext& context,
                                                     const MediaBufferRef& buffer)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegNodeRuntime pushToAllOutputs failed: buffer is null"));
    }

    std::vector<MediaChannel*> targets;
    for (MediaChannel* channel : context.outputChannels(nodeId())) {
        if (!channel) {
            continue;
        }

        targets.push_back(channel);
    }

    if (targets.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime pushToAllOutputs failed: no output channel"));
    }

    return transferOrDefer(context, targets, buffer, "push_all");
}

::media::Status FFmpegNodeRuntime::broadcastControlToAllOutputs(MediaGraphExecutionContext& context,
                                                                 const MediaBufferRef& buffer)
{
    if (!isControlBroadcastBuffer(buffer)) {
        std::ostringstream out;
        out << "FFmpegNodeRuntime broadcastControlToAllOutputs failed: expected control buffer "
            << mediaGraphDiagnosticDescribeBuffer(buffer);
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(out.str()));
    }

    return pushToAllOutputs(context, buffer);
}

::media::Status FFmpegNodeRuntime::pushToMatchingOutputs(MediaGraphExecutionContext& context,
                                                          const MediaBufferRef& buffer,
                                                          MediaStreamKind streamKind,
                                                          int streamIndex,
                                                          RouteMatchPolicy policy)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegNodeRuntime pushToMatchingOutputs failed: buffer is null"));
    }

    std::vector<MediaChannel*> targets;
    for (MediaChannel* channel : context.outputChannels(nodeId())) {
        if (!channel) {
            continue;
        }

        const auto& binding = channel->binding();
        const auto& format = channel->formatDescriptor();
        const bool streamKindMatches =
            binding.streamKind == MediaStreamKind::Any ||
            binding.streamKind == MediaStreamKind::Unknown ||
            binding.streamKind == streamKind;
        const bool streamIndexMatches =
            streamIndex == invalidMediaStreamIndex ||
            !format.hasStreamIndex() ||
            format.streamIndex == streamIndex;

        if (streamKindMatches && streamIndexMatches) {
            targets.push_back(channel);
        }
    }

    if (!targets.empty()) {
        return transferOrDefer(context, targets, buffer, "push_match");
    }
    if (policy == RouteMatchPolicy::AllowDrop) {
        return ::media::Status::success();
    }

    std::ostringstream out;
    out << "FFmpegNodeRuntime pushToMatchingOutputs failed: no matching output channel stream="
        << mediaGraphDiagnosticStreamKindName(streamKind)
        << " stream_index=" << streamIndex
        << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
    return ::media::Status::failure(::media::ErrorInfo::notInitialized(out.str()));
}

::media::Status FFmpegNodeRuntime::transferOrDefer(MediaGraphExecutionContext& context,
                                                    const std::vector<MediaChannel*>& channels,
                                                    const MediaBufferRef& buffer,
                                                    const char* action)
{
    if (m_pendingTransfer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::wouldBlock("FFmpegNodeRuntime output transfer is pending"));
    }
    for (MediaChannel* channel : channels) {
        auto typeStatus = validateChannelBufferType(*channel, buffer, action);
        if (!typeStatus) {
            return typeStatus;
        }
    }

    for (std::size_t index = 0; index < channels.size(); ++index) {
        MediaChannel* channel = channels[index];
        const MediaQueuePushOutcome outcome = channel->pushOutcome(buffer);
        if (outcome == MediaQueuePushOutcome::WouldBlock) {
            m_pendingTransfer = PendingTransfer{ buffer, channels, index };
            return ::media::Status::failure(
                ::media::ErrorInfo::wouldBlock("FFmpegNodeRuntime output channel would block"));
        }
        if (outcome == MediaQueuePushOutcome::Aborted) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::internalError("FFmpegNodeRuntime output channel aborted"));
        }
        if (outcome == MediaQueuePushOutcome::Closed) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::cancelled("FFmpegNodeRuntime output channel closed"));
        }
        if (outcome == MediaQueuePushOutcome::Dropped) {
            continue;
        }
        logEdgeTransfer(context, MediaGraphDiagnosticPhase::RuntimeEdge, action,
                        nodeId(), name(), *channel, buffer);
    }
    return ::media::Status::success();
}

::media::Status FFmpegNodeRuntime::drainPendingTransfers(MediaGraphExecutionContext& context,
                                                          bool& waiting)
{
    waiting = false;
    while (m_pendingTransfer) {
        PendingTransfer& transfer = *m_pendingTransfer;
        MediaChannel* channel = transfer.channels[transfer.nextChannel];
        const MediaQueuePushOutcome outcome = channel->pushOutcome(transfer.buffer);
        if (outcome == MediaQueuePushOutcome::WouldBlock) {
            waiting = true;
            return ::media::Status::success();
        }
        if (outcome == MediaQueuePushOutcome::Aborted) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::internalError("FFmpegNodeRuntime pending output channel aborted"));
        }
        if (outcome == MediaQueuePushOutcome::Closed) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::cancelled("FFmpegNodeRuntime pending output channel closed"));
        }
        if (outcome == MediaQueuePushOutcome::Accepted) {
            logEdgeTransfer(context, MediaGraphDiagnosticPhase::RuntimeEdge, "pending_emit",
                            nodeId(), name(), *channel, transfer.buffer);
        }
        ++transfer.nextChannel;
        if (transfer.nextChannel == transfer.channels.size()) {
            m_pendingTransfer.reset();
        }
    }
    return ::media::Status::success();
}

std::vector<MediaChannel*> FFmpegNodeRuntime::outputChannels(MediaGraphExecutionContext& context)
{
    return context.outputChannels(nodeId());
}

} // namespace media::ffmpeg::graph
