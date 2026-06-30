#include "internal/graph/nodes/FFmpegNodeRuntime.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

#include <sstream>
#include <utility>

namespace media::ffmpeg::graph {
namespace {

void logEdgeTransfer(MediaGraphExecutionContext& context,
                     MediaGraphDiagnosticPhase phase,
                     const char* action,
                     MediaNodeId nodeId,
                     const std::string& nodeName,
                     const MediaChannel& channel,
                     const MediaBufferRef& buffer)
{
    // sampling key: node + edge + action
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
                                           std::string fallback) const
{
    const MediaNodeOptions* options = nodeOptions(context);
    return options ? options->value(key, std::move(fallback)) : std::move(fallback);
}

::media::Result<MediaBufferRef> FFmpegNodeRuntime::popInput(MediaGraphExecutionContext& context,
                                                             const std::string& portName)
{
    MediaChannel* channel = context.findInputChannel(nodeId(), portName);
    if (!channel) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime popInput failed: input channel not found"));
    }

    MediaBufferRef buffer;
    auto status = channel->pop(buffer);
    if (!status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
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
    for (MediaChannel* channel : context.inputChannels(nodeId())) {
        if (!channel) {
            continue;
        }

        MediaBufferRef buffer;
        if (channel->tryPop(buffer)) {
            logEdgeTransfer(context,
                            MediaGraphDiagnosticPhase::RuntimeEdge,
                            "try_pop",
                            nodeId(),
                            name(),
                            *channel,
                            buffer);
            return ::media::Result<MediaBufferRef>::success(std::move(buffer));
        }
    }

    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::notInitialized("FFmpegNodeRuntime tryPopFirstInput failed: no input buffer available"));
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

::media::Status FFmpegNodeRuntime::pushToMatchingOutputs(MediaGraphExecutionContext& context,
                                                          const MediaBufferRef& buffer,
                                                          MediaStreamKind streamKind,
                                                          int streamIndex)
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

    if (!pushed) {
        return pushToAllOutputs(context, buffer);
    }

    return ::media::Status::success();
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
    auto buffer = tryPopFirstInput(context);
    if (!buffer) {
        return ::media::Status::success();
    }

    return pushToAllOutputs(context, buffer.value());
}

std::vector<MediaChannel*> FFmpegNodeRuntime::outputChannels(MediaGraphExecutionContext& context)
{
    return context.outputChannels(nodeId());
}

} // namespace media::ffmpeg::graph
