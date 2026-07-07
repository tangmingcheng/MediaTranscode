#include "internal/graph/nodes/FFmpegNodeRuntime.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

bool isWildcardStream(MediaStreamKind kind) noexcept
{
    return kind == MediaStreamKind::Any || kind == MediaStreamKind::Unknown;
}

bool isWildcardPayload(MediaPayloadKind kind) noexcept
{
    return kind == MediaPayloadKind::Unknown;
}

bool isBypassControlBuffer(const MediaChannel& channel, const MediaBufferRef& buffer) noexcept
{
    return buffer &&
        channel.policy().queuePolicy.allowFlushControlBypass &&
        buffer->streamKind() == MediaStreamKind::Control &&
        buffer->payloadKind() == MediaPayloadKind::ControlSignal &&
        (buffer->isEof() || buffer->isFlush());
}

bool isControlBroadcastBuffer(const MediaBufferRef& buffer) noexcept
{
    return buffer &&
        buffer->streamKind() == MediaStreamKind::Control &&
        buffer->payloadKind() == MediaPayloadKind::ControlSignal;
}

::media::Status validateChannelBufferType(const MediaChannel& channel,
                                          const MediaBufferRef& buffer,
                                          const char* action)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument(std::string(action) + " failed: buffer is null"));
    }

    if (isBypassControlBuffer(channel, buffer)) {
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
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
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
            return ::media::Result<std::optional<MediaBufferRef>>::success(std::move(buffer));
        }
    }

    return ::media::Result<std::optional<MediaBufferRef>>::success(std::nullopt);
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

    bool pushed = false;
    for (MediaChannel* channel : context.outputChannels(nodeId())) {
        if (!channel || channel->binding().from.portId != port->id) {
            continue;
        }

        auto typeStatus = validateChannelBufferType(*channel, buffer, "emitOutput");
        if (!typeStatus) {
            return typeStatus;
        }

        auto status = channel->push(buffer);
        if (!status) {
            return status;
        }

        logEdgeTransfer(context,
                        MediaGraphDiagnosticPhase::RuntimeEdge,
                        "emit",
                        nodeId(),
                        name(),
                        *channel,
                        buffer);
        pushed = true;
    }

    if (!pushed) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime emitOutput failed: no output channel"));
    }

    return ::media::Status::success();
}

::media::Status FFmpegNodeRuntime::pushToAllOutputs(MediaGraphExecutionContext& context,
                                                     const MediaBufferRef& buffer)
{
    if (!buffer) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FFmpegNodeRuntime pushToAllOutputs failed: buffer is null"));
    }

    bool pushed = false;
    for (MediaChannel* channel : context.outputChannels(nodeId())) {
        if (!channel) {
            continue;
        }

        auto typeStatus = validateChannelBufferType(*channel, buffer, "pushToAllOutputs");
        if (!typeStatus) {
            return typeStatus;
        }

        auto status = channel->push(buffer);
        if (!status) {
            return status;
        }

        logEdgeTransfer(context,
                        MediaGraphDiagnosticPhase::RuntimeEdge,
                        "push_all",
                        nodeId(),
                        name(),
                        *channel,
                        buffer);
        pushed = true;
    }

    if (!pushed) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime pushToAllOutputs failed: no output channel"));
    }

    return ::media::Status::success();
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

    bool pushed = false;
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
            auto typeStatus = validateChannelBufferType(*channel, buffer, "pushToMatchingOutputs");
            if (!typeStatus) {
                return typeStatus;
            }

            auto status = channel->push(buffer);
            if (!status) {
                return status;
            }

            logEdgeTransfer(context,
                            MediaGraphDiagnosticPhase::RuntimeEdge,
                            "push_match",
                            nodeId(),
                            name(),
                            *channel,
                            buffer);
            pushed = true;
        }
    }

    if (pushed || policy == RouteMatchPolicy::AllowDrop) {
        return ::media::Status::success();
    }

    std::ostringstream out;
    out << "FFmpegNodeRuntime pushToMatchingOutputs failed: no matching output channel stream="
        << mediaGraphDiagnosticStreamKindName(streamKind)
        << " stream_index=" << streamIndex
        << " " << mediaGraphDiagnosticDescribeBuffer(buffer);
    return ::media::Status::failure(::media::ErrorInfo::notInitialized(out.str()));
}

::media::Status FFmpegNodeRuntime::forward(MediaGraphExecutionContext& context,
                                            const std::string& inputPortName,
                                            const std::string& outputPortName)
{
    auto buffer = popInput(context, inputPortName);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    return emitOutput(context, outputPortName, std::move(buffer).value());
}

::media::Status FFmpegNodeRuntime::forwardFirstInputToAllOutputs(MediaGraphExecutionContext& context)
{
    auto buffer = tryPopFirstInputOptional(context);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    if (!buffer.value()) {
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, *buffer.value());
}

std::vector<MediaChannel*> FFmpegNodeRuntime::outputChannels(MediaGraphExecutionContext& context)
{
    return context.outputChannels(nodeId());
}

} // namespace media::ffmpeg::graph
