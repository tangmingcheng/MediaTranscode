#include "internal/graph/nodes/output/FileOutputNode.h"

#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/MediaOutputByteSinkBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegAvioOutputByteSink.h"
#include "internal/graph/runtime/ffmpeg/FFmpegGraphError.h"

extern "C" {
#include <libavutil/error.h>
#include <libavformat/avio.h>
}

#include <utility>

namespace media::ffmpeg::graph {
namespace {

::media::Result<MediaBufferRef> createFormatContextResource(
    const std::string& url,
    const std::string& formatName)
{
    if (formatName.empty()) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument(
                "FFmpeg format-context output requires format"));
    }
    AVFormatContext* raw = nullptr;
    const int allocRet = avformat_alloc_output_context2(
        &raw, nullptr, formatName.c_str(), url.c_str());
    if (allocRet < 0 || !raw) {
        return ::media::Result<MediaBufferRef>::failure(
            FFmpegGraphError::fromCode(
                allocRet < 0 ? allocRet : AVERROR_UNKNOWN,
                "avformat_alloc_output_context2"));
    }
    ::media::ffmpeg::OutputFormatContextPtr context(raw);
    if (context->oformat && !(context->oformat->flags & AVFMT_NOFILE)) {
        const int openRet = avio_open(&context->pb, url.c_str(), AVIO_FLAG_WRITE);
        if (openRet < 0) {
            return ::media::Result<MediaBufferRef>::failure(
                FFmpegGraphError::fromCode(openRet, "avio_open"));
        }
    }
    return FFmpegBufferFactory::wrapOutputFormatContext(std::move(context));
}

::media::Result<MediaBufferRef> createByteSinkResource(const std::string& url)
{
    auto sink = FFmpegAvioOutputByteSink::open(url, AVIO_FLAG_WRITE);
    if (!sink) return ::media::Result<MediaBufferRef>::failure(sink.error());
    auto buffer = MediaOutputByteSinkBuffer::create(std::move(sink).value());
    if (!buffer) return ::media::Result<MediaBufferRef>::failure(buffer.error());
    return ::media::Result<MediaBufferRef>::success(
        MediaBufferRef(std::move(buffer).value()));
}

} // namespace

FileOutputNode::FileOutputNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "FileOutputNode")
{
}

MediaNodeKind FileOutputNode::staticKind() noexcept
{
    return MediaNodeKind::FileOutput;
}

::media::Result<MediaNodeProcessResult> FileOutputNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return processFinished();
    }

    if (!m_resource) {
        auto resource = createResource(context);
        if (!resource) {
            return ::media::Result<MediaNodeProcessResult>::failure(resource.error());
        }
        m_resource = std::move(resource).value();
    }
    auto pushStatus = pushToAllOutputs(context, m_resource);
    if (!pushStatus && pushStatus.error().code != ::media::ErrorCode::WouldBlock) {
        return processProgress(pushStatus);
    }
    m_resource.reset();
    m_emitted = true;
    return processProgress(std::move(pushStatus));
}

::media::Result<MediaBufferRef> FileOutputNode::createResource(
    MediaGraphExecutionContext& context)
{
    auto kindOption = requiredNodeOption(
        nodeOptions(context), "FileOutputNode",
        MediaTranscodeOptionKey::OutputResourceKind);
    if (!kindOption) return ::media::Result<MediaBufferRef>::failure(kindOption.error());
    auto kind = parseMediaOutputResourceKindOption(kindOption.value());
    if (!kind) return ::media::Result<MediaBufferRef>::failure(kind.error());
    auto url = requiredNodeOption(nodeOptions(context), "FileOutputNode", "url");
    if (!url) return ::media::Result<MediaBufferRef>::failure(url.error());
    switch (kind.value()) {
    case MediaOutputResourceKind::FFmpegFormatContext:
        return createFormatContextResource(url.value(), nodeOption(context, "format"));
    case MediaOutputResourceKind::ByteSink:
        return createByteSinkResource(url.value());
    }
    return ::media::Result<MediaBufferRef>::failure(
        ::media::ErrorInfo::unsupported("FileOutputNode unsupported resource kind"));
}

} // namespace media::ffmpeg::graph
