#include "internal/graph/nodes/input/RealtimeInputNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <string>

namespace media::ffmpeg::graph {
namespace {

std::string millisecondsAsMicrosecondsText(const std::string& milliseconds)
{
    if (milliseconds.empty()) {
        return {};
    }
    const int parsed = std::stoi(milliseconds);
    return parsed > 0 ? std::to_string(parsed * 1000) : std::string();
}

void setDictionaryOption(AVDictionary** dictionary,
                         const std::string& key,
                         const std::string& value)
{
    if (!value.empty()) {
        av_dict_set(dictionary, key.c_str(), value.c_str(), 0);
    }
}

} // namespace

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

    const std::string url = nodeOption(context, "url");
    if (url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RealtimeInputNode requires node option: url"));
    }

    AVDictionary* inputOptions = nullptr;
    setDictionaryOption(&inputOptions, "rtsp_transport", nodeOption(context, "input.rtsp_transport", "tcp"));
    setDictionaryOption(&inputOptions, "stimeout", millisecondsAsMicrosecondsText(nodeOption(context, "input.open_timeout_ms")));
    setDictionaryOption(&inputOptions, "rw_timeout", millisecondsAsMicrosecondsText(nodeOption(context, "input.read_timeout_ms")));
    setDictionaryOption(&inputOptions, "timeout", millisecondsAsMicrosecondsText(nodeOption(context, "input.read_timeout_ms")));
    setDictionaryOption(&inputOptions, "analyzeduration", nodeOption(context, "input.analyze_duration_us"));
    setDictionaryOption(&inputOptions, "probesize", nodeOption(context, "input.probe_size_bytes"));
    if (nodeOption(context, "input.low_latency", "1") == "1") {
        setDictionaryOption(&inputOptions, "fflags", "nobuffer");
        setDictionaryOption(&inputOptions, "flags", "low_delay");
    }

    AVFormatContext* raw = nullptr;
    const int openRet = avformat_open_input(&raw, url.c_str(), nullptr, &inputOptions);
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
