#include "internal/graph/nodes/output/FileOutputNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
}

namespace media::ffmpeg::graph {

FileOutputNode::FileOutputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileOutputNode")
{
}

MediaNodeKind FileOutputNode::staticKind() noexcept
{
    return MediaNodeKind::FileOutput;
}

::media::Status FileOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return ::media::Status::success();
    }

    auto status = openOutput(context);
    if (!status) {
        return status;
    }

    auto buffer = FFmpegBufferFactory::wrapOutputFormatContext(std::move(m_context));
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    auto pushStatus = pushToAllOutputs(context, buffer.value());
    if (!pushStatus) {
        return pushStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

::media::Status FileOutputNode::openOutput(MediaGraphExecutionContext& context)
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
            ::media::ErrorInfo::invalidArgument("FileOutputNode requires node option: url or path"));
    }

    const std::string formatName = nodeOption(context, "format");
    AVFormatContext* raw = nullptr;
    const int allocRet = avformat_alloc_output_context2(
        &raw,
        nullptr,
        formatName.empty() ? nullptr : formatName.c_str(),
        url.c_str());
    if (allocRet < 0 || !raw) {
        return FFmpegGraphError::statusFromCode(allocRet < 0 ? allocRet : AVERROR_UNKNOWN,
                                                "avformat_alloc_output_context2");
    }

    m_context.reset(raw);

    if (m_context->oformat && !(m_context->oformat->flags & AVFMT_NOFILE)) {
        const int openRet = avio_open(&m_context->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (openRet < 0) {
            return FFmpegGraphError::statusFromCode(openRet, "avio_open");
        }
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
