#include "internal/graph/nodes/input/FileInputNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

#include <utility>

namespace media::ffmpeg::graph {

FileInputNode::FileInputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileInputNode")
{
}

MediaNodeKind FileInputNode::staticKind() noexcept
{
    return MediaNodeKind::FileInput;
}

::media::Status FileInputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
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

    auto pushStatus = pushToAllOutputs(context, buffer.value(), ControlBroadcastPolicy::AllowAnyBuffer);
    if (!pushStatus) {
        return pushStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

::media::Status FileInputNode::openInput(MediaGraphExecutionContext& context)
{
    if (m_context) {
        return ::media::Status::success();
    }

    std::string url = nodeOption(context, "url");
    if (url.empty()) {
        url = nodeOption(context, "path");
    }

    if (url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("FileInputNode requires node option: url or path"));
    }

    AVFormatContext* raw = nullptr;
    const int openRet = avformat_open_input(&raw, url.c_str(), nullptr, nullptr);
    if (openRet < 0) {
        return FFmpegGraphError::statusFromCode(openRet, "avformat_open_input");
    }

    m_context.reset(raw);

    const int streamRet = avformat_find_stream_info(m_context.get(), nullptr);
    if (streamRet < 0) {
        return FFmpegGraphError::statusFromCode(streamRet, "avformat_find_stream_info");
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
