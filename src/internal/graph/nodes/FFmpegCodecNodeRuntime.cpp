#include "internal/graph/nodes/FFmpegCodecNodeRuntime.h"

#include <utility>
#include "internal/graph/runtime/ffmpeg/FFmpegCodecParametersMaterializer.h"

namespace media::ffmpeg::graph {

FFmpegCodecNodeRuntime::FFmpegCodecNodeRuntime(MediaNodeId nodeId, MediaNodeKind kind, std::string name)
    : FFmpegNodeRuntime(nodeId, kind, std::move(name))
{
}

::media::Status FFmpegCodecNodeRuntime::start(MediaGraphExecutionContext& context)
{
    resetCodecContext();
    return FFmpegNodeRuntime::start(context);
}

::media::Status FFmpegCodecNodeRuntime::stop(MediaGraphExecutionContext& context)
{
    auto status = FFmpegNodeRuntime::stop(context);
    resetCodecContext();
    return status;
}

void FFmpegCodecNodeRuntime::abort(MediaGraphExecutionContext& context) noexcept
{
    FFmpegNodeRuntime::abort(context);
    resetCodecContext();
}

void FFmpegCodecNodeRuntime::resetCodecContext() noexcept
{
    m_codecContext = nullptr;
    m_codecContextOwner.reset();
    m_codecParametersSnapshot.reset();
    m_contextMetadataPublished = false;
    m_parametersMetadataPublished = false;
}

bool FFmpegCodecNodeRuntime::tryBindCodecContext(const MediaBufferRef& buffer) noexcept
{
    auto* codecBuffer = dynamic_cast<FFmpegCodecContextBuffer*>(buffer.get());
    if (!codecBuffer || !codecBuffer->context()) {
        return false;
    }

    m_codecContextOwner = buffer;
    m_codecContext = codecBuffer->context();
    return true;
}

AVCodecContext* FFmpegCodecNodeRuntime::codecContext() noexcept
{
    return m_codecContext;
}

const AVCodecContext* FFmpegCodecNodeRuntime::codecContext() const noexcept
{
    return m_codecContext;
}

bool FFmpegCodecNodeRuntime::hasCodecContext() const noexcept
{
    return m_codecContext != nullptr;
}

bool FFmpegCodecNodeRuntime::codecMetadataPublished() const noexcept
{
    return m_contextMetadataPublished && m_parametersMetadataPublished;
}

::media::Status FFmpegCodecNodeRuntime::publishCodecMetadata(MediaGraphExecutionContext& context)
{
    if (!m_codecContext || !m_codecContextOwner)
        return ::media::Status::failure(::media::ErrorInfo::notInitialized("codec metadata requires a bound codec"));
    if (!m_contextMetadataPublished) {
        if (context.findOutputChannel(nodeId(), "codec")) {
            auto status = emitOutput(context, "codec", m_codecContextOwner);
            if (status || retainsPendingOutput(m_codecContextOwner)) m_contextMetadataPublished = true;
            if (!status) return status;
        } else m_contextMetadataPublished = true;
    }
    if (!m_parametersMetadataPublished) {
        if (context.findOutputChannel(nodeId(), "codec_parameters")) {
            if (!m_codecParametersSnapshot) {
                auto captured = FFmpegCodecParametersMaterializer::snapshot(*m_codecContext);
                if (!captured) return ::media::Status::failure(captured.error());
                m_codecParametersSnapshot = std::move(captured).value();
            }
            auto status = emitOutput(context, "codec_parameters", m_codecParametersSnapshot);
            if (status || retainsPendingOutput(m_codecParametersSnapshot)) m_parametersMetadataPublished = true;
            if (!status) return status;
        } else m_parametersMetadataPublished = true;
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
