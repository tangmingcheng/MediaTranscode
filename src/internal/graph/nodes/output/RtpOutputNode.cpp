#include "internal/graph/nodes/output/RtpOutputNode.h"

#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <string>

namespace media::ffmpeg::graph {

RtpOutputNode::RtpOutputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "RtpOutputNode")
{
}

MediaNodeKind RtpOutputNode::staticKind() noexcept
{
    return MediaNodeKind::RtpOutput;
}

::media::Status RtpOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_formatEmitted) {
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

    auto pushed = emitOutput(context, "format", buffer.value());
    if (!pushed) {
        return pushed;
    }

    m_formatEmitted = true;
    return ::media::Status::success();
}

::media::Status RtpOutputNode::stop(MediaGraphExecutionContext& context)
{
    m_context.reset();
    m_formatEmitted = false;
    return FFmpegNodeRuntime::stop(context);
}

::media::Status RtpOutputNode::openOutput(MediaGraphExecutionContext& context)
{
    if (m_context) {
        return ::media::Status::success();
    }

    const std::string url = nodeOption(context, "url");
    if (url.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("RtpOutputNode requires node option: url"));
    }

    AVFormatContext* raw = nullptr;
    const int allocRet = avformat_alloc_output_context2(&raw, nullptr, "rtp", url.c_str());
    if (allocRet < 0 || !raw) {
        return FFmpegGraphError::statusFromCode(allocRet < 0 ? allocRet : AVERROR_UNKNOWN,
                                                "avformat_alloc_output_context2(rtp)");
    }

    m_context.reset(raw);
    const int packetSize = std::stoi(nodeOption(context, "rtp.packet_size", "1200"));
    if (packetSize > 0) {
        m_context->packet_size = packetSize;
    }

    if (m_context->oformat && !(m_context->oformat->flags & AVFMT_NOFILE)) {
        const int openRet = avio_open(&m_context->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (openRet < 0) {
            return FFmpegGraphError::statusFromCode(openRet, "avio_open(rtp)");
        }
    }

    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
