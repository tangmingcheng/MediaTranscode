#include "internal/graph/nodes/output/SdpWriterNode.h"

#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <array>
#include <fstream>
#include <string>
#include <vector>

namespace media::ffmpeg::graph {
namespace {

int mediaOrder(const AVFormatContext* context) noexcept
{
    if (!context || context->nb_streams == 0 || !context->streams[0] || !context->streams[0]->codecpar) {
        return 3;
    }
    switch (context->streams[0]->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        return 0;
    case AVMEDIA_TYPE_AUDIO:
        return 1;
    default:
        return 2;
    }
}

} // namespace

SdpWriterNode::SdpWriterNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "SdpWriterNode")
{
}

MediaNodeKind SdpWriterNode::staticKind() noexcept
{
    return MediaNodeKind::SdpWriter;
}

::media::Result<MediaNodeProcessResult> SdpWriterNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_written) {
        return processFinished();
    }
    auto configured = configureExpectedContexts(context);
    if (!configured) {
        return processProgress(configured);
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Result<MediaNodeProcessResult>::failure(input.error());
    }
    if (!input.value()) {
        return processWaiting();
    }

    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.value()->get());
    AVFormatContext* formatContext = formatBuffer ? formatBuffer->context() : nullptr;
    if (!formatContext) {
        return ::media::Result<MediaNodeProcessResult>::failure(
            ::media::ErrorInfo::invalidArgument("SdpWriterNode expected FFmpeg format context"));
    }
    m_formatBuffers.push_back(*input.value());
    auto status = writeIfReady(context);
    return m_written ? processFinished(status) : processProgress(status);
}

::media::Status SdpWriterNode::configureExpectedContexts(MediaGraphExecutionContext& context)
{
    if (m_expectedContextsBound) {
        return ::media::Status::success();
    }
    auto expected = requiredPositiveIntNodeOption(nodeOptions(context), "SdpWriterNode", "sdp.expected_contexts");
    if (!expected) {
        return ::media::Status::failure(expected.error());
    }
    m_expectedContexts = expected.value();
    m_expectedContextsBound = true;
    return ::media::Status::success();
}

::media::Status SdpWriterNode::writeIfReady(MediaGraphExecutionContext& context)
{
    if (static_cast<int>(m_formatBuffers.size()) < m_expectedContexts) {
        return ::media::Status::success();
    }
    const std::string path = nodeOption(context, "path");
    if (path.empty()) {
        m_written = true;
        return ::media::Status::success();
    }

    std::vector<AVFormatContext*> contexts;
    contexts.reserve(m_formatBuffers.size());
    for (const MediaBufferRef& buffer : m_formatBuffers) {
        auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
        AVFormatContext* formatContext = formatBuffer ? formatBuffer->context() : nullptr;
        if (!formatContext) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("SdpWriterNode expected buffered FFmpeg format context"));
        }
        contexts.push_back(formatContext);
    }
    std::stable_sort(contexts.begin(), contexts.end(), [](const AVFormatContext* left, const AVFormatContext* right) {
        return mediaOrder(left) < mediaOrder(right);
    });

    std::array<char, 16384> text {};
    const int ret = av_sdp_create(contexts.data(), static_cast<int>(contexts.size()), text.data(), static_cast<int>(text.size()));
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
    m_expectedContextsBound = false;
    m_expectedContexts = 1;
    m_formatBuffers.clear();
    return FFmpegNodeRuntime::stop(context);
}

void SdpWriterNode::abort(MediaGraphExecutionContext& context) noexcept
{
    m_written = false;
    m_expectedContextsBound = false;
    m_expectedContexts = 1;
    m_formatBuffers.clear();
    FFmpegNodeRuntime::abort(context);
}

} // namespace media::ffmpeg::graph
