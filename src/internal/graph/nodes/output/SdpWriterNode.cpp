#include "internal/graph/nodes/output/SdpWriterNode.h"

#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <algorithm>
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

    const std::string path = nodeOption(context, "path");
    if (path.empty()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("SdpWriterNode requires node option: path"));
    }

    auto input = tryPopFirstInputOptional(context);
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    auto* format = dynamic_cast<FFmpegFormatContextBuffer*>(input.value()->get());
    AVFormatContext* formatContext = format ? format->context() : nullptr;
    if (!formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("SdpWriterNode expected output format context"));
    }

    const bool alreadySeen = std::any_of(m_contextBuffers.begin(),
                                         m_contextBuffers.end(),
                                         [formatContext](const MediaBufferRef& existing) {
                                             auto* existingFormat = dynamic_cast<FFmpegFormatContextBuffer*>(existing.get());
                                             return existingFormat && existingFormat->context() == formatContext;
                                         });
    if (!alreadySeen) {
        m_contextBuffers.push_back(*input.value());
    }

    int expectedContexts = 1;
    try {
        expectedContexts = std::max(1, std::stoi(nodeOption(context, "sdp.expected_contexts", "1")));
    } catch (...) {
        expectedContexts = 1;
    }
    if (m_contextBuffers.size() < static_cast<std::size_t>(expectedContexts)) {
        return ::media::Status::success();
    }
    return writeSdp(context, path);
}

::media::Status SdpWriterNode::writeSdp(MediaGraphExecutionContext&, const std::string& path)
{
    std::vector<AVFormatContext*> contexts;
    contexts.reserve(m_contextBuffers.size());
    for (const MediaBufferRef& buffer : m_contextBuffers) {
        auto* format = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
        AVFormatContext* context = format ? format->context() : nullptr;
        if (!context) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument("SdpWriterNode stored an invalid output format context"));
        }
        contexts.push_back(context);
    }

    char sdp[4096] = {};
    const int ret = av_sdp_create(contexts.data(), static_cast<int>(contexts.size()), sdp, sizeof(sdp));
    if (ret < 0) {
        return FFmpegGraphError::statusFromCode(ret, "av_sdp_create");
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("SdpWriterNode failed to open SDP path"));
    }

    out << sdp;
    m_written = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
