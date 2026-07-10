#include "internal/graph/nodes/packet/PacketSourceConfigNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

void packetSourceConfigLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("packet_source_config.") + message);
}

} // namespace

PacketSourceConfigNode::PacketSourceConfigNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketSourceConfigNode")
{
}

MediaNodeKind PacketSourceConfigNode::staticKind() noexcept
{
    return MediaNodeKind::PacketSourceConfig;
}

::media::Status PacketSourceConfigNode::stop(MediaGraphExecutionContext& context)
{
    releaseFormatContext();
    return FFmpegNodeRuntime::stop(context);
}

void PacketSourceConfigNode::abort(MediaGraphExecutionContext& context) noexcept
{
    releaseFormatContext();
    FFmpegNodeRuntime::abort(context);
}

::media::Result<MediaNodeProcessResult> PacketSourceConfigNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return processFinished();
    }

    if (!m_formatContextOwner) {
        auto bindStatus = bindFormatContext(context);
        if (!bindStatus) {
            return processProgress(bindStatus);
        }
        if (!m_formatContextOwner) {
            return processWaiting();
        }
    }

    if (m_sourceStreamIndex == invalidMediaStreamIndex) {
        auto streamStatus = bindSourceStream(context);
        if (!streamStatus) {
            return processProgress(streamStatus);
        }
    }

    return processFinished(emitSourceConfig(context));
}

void PacketSourceConfigNode::releaseFormatContext() noexcept
{
    m_formatContextOwner.reset();
    m_sourceStream = nullptr;
    m_streamKind = MediaStreamKind::Unknown;
    m_sourceStreamIndex = invalidMediaStreamIndex;
    m_emitted = false;
}

::media::Status PacketSourceConfigNode::bindFormatContext(MediaGraphExecutionContext& context)
{
    auto input = tryPopInputOptional(context, "format");
    if (!input) {
        return ::media::Status::failure(input.error());
    }
    if (!input.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef buffer = *input.value();
    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(buffer.get());
    if (!formatBuffer || !formatBuffer->inputSnapshotComplete()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(buffer);
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, "bind_format_context");
    return ::media::Status::success();
}

::media::Status PacketSourceConfigNode::bindSourceStream(MediaGraphExecutionContext& context)
{
    auto streamKind = requiredStreamKindNodeOption(nodeOptions(context),
                                                   "PacketSourceConfigNode",
                                                   MediaTranscodeOptionKey::PacketStreamKind);
    if (!streamKind) {
        return ::media::Status::failure(streamKind.error());
    }
    auto streamIndex = requiredNonNegativeIntNodeOption(nodeOptions(context),
                                                        "PacketSourceConfigNode",
                                                        MediaTranscodeOptionKey::PacketSourceStreamIndex);
    if (!streamIndex) {
        return ::media::Status::failure(streamIndex.error());
    }

    if (!m_formatContextOwner) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    const auto* formatBuffer = dynamic_cast<const FFmpegFormatContextBuffer*>(m_formatContextOwner.get());
    m_sourceStream = formatBuffer ? formatBuffer->inputStreamSnapshot(index) : nullptr;
    if (!m_sourceStream || m_sourceStream->streamKind != streamKind.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode planner source stream kind does not match stream"));
    }

    m_streamKind = streamKind.value();
    m_sourceStreamIndex = index;

    std::ostringstream out;
    out << "bind_source_stream stream=" << mediaGraphDiagnosticStreamKindName(m_streamKind)
        << " index=" << m_sourceStreamIndex;
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, out.str());
    return ::media::Status::success();
}

::media::Status PacketSourceConfigNode::emitSourceConfig(MediaGraphExecutionContext& context)
{
    if (!m_sourceStream || m_sourceStreamIndex == invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode requires source stream before emitting config"));
    }

    auto buffer = FFmpegBufferFactory::cloneCodecParameters(*m_sourceStream);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }
    if (buffer.value()->streamKind() != m_streamKind) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode cloned codec parameters stream kind mismatch"));
    }

    std::ostringstream out;
    out << "emit_source_config stream=" << mediaGraphDiagnosticStreamKindName(m_streamKind)
        << " index=" << m_sourceStreamIndex
        << " time_base=" << m_sourceStream->time.timeBase.num << "/" << m_sourceStream->time.timeBase.den
        << " codec_id=" << m_sourceStream->codecParameters->codec_id;
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, out.str());

    auto emitStatus = emitOutput(context, "codec", buffer.value());
    if (!emitStatus) {
        return emitStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
