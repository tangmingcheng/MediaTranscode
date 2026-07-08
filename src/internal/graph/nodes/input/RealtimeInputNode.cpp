#include "internal/graph/nodes/input/RealtimeInputNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"
#include "internal/graph/runtime/ffmpeg/FFmpegRealtimeInputOptions.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <string>

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

    auto status = openInput(context);
    if (!status) {
        return status;
    }

    auto buffer = FFmpegBufferFactory::wrapInputFormatContext(std::move(m_context));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    auto pushed = emitOutput(context, "format", buffer.value());
    if (!pushed) {
        return pushed;
    }

    m_formatEmitted = true;
    return ::media::Status::success();
}

::media::Status RealtimeInputNode::stop(MediaGraphExecutionContext& context)
{
    m_context.reset();
    m_formatEmitted = false;
    return FFmpegNodeRuntime::stop(context);
}

void RealtimeInputNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_context.reset();
    m_formatEmitted = false;
    FFmpegNodeRuntime::abort(context);
}

::media::Status RealtimeInputNode::openInput(MediaGraphExecutionContext& context)
{
    if (m_context) {
        return ::media::Status::success();
    }

    const MediaNodeOptions* options = nodeOptions(context);
    auto url = requiredNodeOption(options, "RealtimeInputNode", "url");
    if (!url) {
        return ::media::Status::failure(url.error());
    }
    auto openTimeoutMs = requiredPositiveIntNodeOption(options, "RealtimeInputNode", "input.open_timeout_ms");
    if (!openTimeoutMs) {
        return ::media::Status::failure(openTimeoutMs.error());
    }
    auto readTimeoutMs = requiredPositiveIntNodeOption(options, "RealtimeInputNode", "input.read_timeout_ms");
    if (!readTimeoutMs) {
        return ::media::Status::failure(readTimeoutMs.error());
    }
    auto analyzeDurationUs = requiredPositiveIntNodeOption(options, "RealtimeInputNode", "input.analyze_duration_us");
    if (!analyzeDurationUs) {
        return ::media::Status::failure(analyzeDurationUs.error());
    }
    auto probeSizeBytes = requiredPositiveIntNodeOption(options, "RealtimeInputNode", "input.probe_size_bytes");
    if (!probeSizeBytes) {
        return ::media::Status::failure(probeSizeBytes.error());
    }
    auto lowLatency = requiredBoolNodeOption(options, "RealtimeInputNode", "input.low_latency");
    if (!lowLatency) {
        return ::media::Status::failure(lowLatency.error());
    }

    FFmpegRealtimeInputOptions realtimeOptions;
    realtimeOptions.rtspTransport = nodeOption(context, "input.rtsp_transport");
    realtimeOptions.openTimeoutMs = openTimeoutMs.value();
    realtimeOptions.readTimeoutMs = readTimeoutMs.value();
    realtimeOptions.analyzeDurationUs = analyzeDurationUs.value();
    realtimeOptions.probeSizeBytes = probeSizeBytes.value();
    realtimeOptions.lowLatency = lowLatency.value();

    AVDictionary* inputOptions = nullptr;
    applyFFmpegRealtimeInputOptions(&inputOptions, realtimeOptions);

    AVFormatContext* raw = nullptr;
    const int openRet = avformat_open_input(&raw, url.value().c_str(), nullptr, &inputOptions);
    if (inputOptions) {
        av_dict_free(&inputOptions);
    }
    if (openRet < 0) {
        return FFmpegGraphError::statusFromCode(openRet, "avformat_open_input(realtime)");
    }

    m_context.reset(raw);
    const int infoRet = avformat_find_stream_info(m_context.get(), nullptr);
    if (infoRet < 0) {
        return FFmpegGraphError::statusFromCode(infoRet, "avformat_find_stream_info(realtime)");
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
