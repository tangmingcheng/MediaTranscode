#include "internal/graph/nodes/output/SdpWriterNode.h"

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <array>
#include <fstream>
#include <string>

namespace media::ffmpeg::graph {

SdpWriterNode::SdpWriterNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "SdpWriterNode")
{
}

MediaNodeKind SdpWriterNode::staticKind() noexcept
{
    return MediaNodeKind::SdpWriter;
}

::media::Status SdpWriterNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_written) {
        return ::media::Status::success();
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.value()->get());
    AVFormatContext* formatContext = formatBuffer ? formatBuffer->context() : nullptr;
    if (!formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("SdpWriterNode expected FFmpeg format context"));
    }

    const std::string path = nodeOption(context, "path");
    if (path.empty()) {
        m_written = true;
        return ::media::Status::success();
    }

    std::array<char, 16384> text {};
    AVFormatContext* contexts[] = { formatContext };
    const int ret = av_sdp_create(contexts, 1, text.data(), static_cast<int>(text.size()));
    if (ret < 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ffmpegFailure("av_sdp_create failed", ret));
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("SdpWriterNode failed to open SDP path: " + path));
    }
    out << text.data();
    if (!out) {
        return ::media::Status::failure(
            ::media::ErrorInfo::ioFailure("SdpWriterNode failed to write SDP path: " + path));
    }

    m_written = true;
    return ::media::Status::success();
}

::media::Status SdpWriterNode::stop(MediaGraphExecutionContext& context)
{
    m_written = false;
    return FFmpegNodeRuntime::stop(context);
}

void SdpWriterNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_written = false;
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
