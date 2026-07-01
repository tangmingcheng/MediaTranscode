#include "internal/graph/nodes/packet/PacketSourceConfigNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <charconv>
#include <cctype>
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

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

::media::Result<int> parseRequiredSourceStreamIndex(const MediaNodeOptions* options)
{
    if (!options || !options->has(MediaTranscodeOptionKey::PacketSourceStreamIndex)) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode requires planner option packet.source_stream_index"));
    }

    const std::string value = options->value(MediaTranscodeOptionKey::PacketSourceStreamIndex);
    int streamIndex = invalidMediaStreamIndex;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, streamIndex);
    if (result.ec != std::errc{} || result.ptr != end || streamIndex < 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode received invalid packet.source_stream_index"));
    }

    return ::media::Result<int>::success(streamIndex);
}

::media::Result<MediaStreamKind> parseRequiredStreamKind(const MediaNodeOptions* options)
{
    if (!options || !options->has(MediaTranscodeOptionKey::PacketStreamKind)) {
        return ::media::Result<MediaStreamKind>::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode requires planner option packet.stream_kind"));
    }

    const std::string value = lowerCopy(options->value(MediaTranscodeOptionKey::PacketStreamKind));
    if (value == "video") {
        return ::media::Result<MediaStreamKind>::success(MediaStreamKind::Video);
    }
    if (value == "audio") {
        return ::media::Result<MediaStreamKind>::success(MediaStreamKind::Audio);
    }

    return ::media::Result<MediaStreamKind>::failure(
        ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode supports packet.stream_kind values: video, audio"));
}

bool streamTypeMatches(MediaStreamKind streamKind, const AVStream* stream) noexcept
{
    if (!stream || !stream->codecpar) {
        return false;
    }
    if (streamKind == MediaStreamKind::Video) {
        return stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
    }
    if (streamKind == MediaStreamKind::Audio) {
        return stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO;
    }
    return false;
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

::media::Status PacketSourceConfigNode::onProcess(MediaGraphExecutionContext& context)
{
    if (m_emitted) {
        return ::media::Status::success();
    }

    if (!m_formatContext) {
        auto bindStatus = bindFormatContext(context);
        if (!bindStatus) {
            return bindStatus;
        }
        if (!m_formatContext) {
            return ::media::Status::success();
        }
    }

    if (m_sourceStreamIndex == invalidMediaStreamIndex) {
        auto streamStatus = bindSourceStream(context);
        if (!streamStatus) {
            return streamStatus;
        }
    }

    return emitSourceConfig(context);
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
    if (!formatBuffer || !formatBuffer->context()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(buffer);
    m_formatContext = formatBuffer->context();
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, "bind_format_context");
    return ::media::Status::success();
}

::media::Status PacketSourceConfigNode::bindSourceStream(MediaGraphExecutionContext& context)
{
    auto streamKind = parseRequiredStreamKind(nodeOptions(context));
    if (!streamKind) {
        return ::media::Status::failure(streamKind.error());
    }
    auto streamIndex = parseRequiredSourceStreamIndex(nodeOptions(context));
    if (!streamIndex) {
        return ::media::Status::failure(streamIndex.error());
    }

    if (!m_formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    if (index < 0 || index >= static_cast<int>(m_formatContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketSourceConfigNode planner source stream index is out of range"));
    }

    const AVStream* stream = m_formatContext->streams[index];
    if (!streamTypeMatches(streamKind.value(), stream)) {
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
    if (!m_formatContext || m_sourceStreamIndex == invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketSourceConfigNode requires source stream before emitting config"));
    }

    AVStream* stream = m_formatContext->streams[m_sourceStreamIndex];
    auto buffer = FFmpegBufferFactory::cloneCodecParameters(stream);
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
        << " time_base=" << stream->time_base.num << "/" << stream->time_base.den
        << " codec_id=" << stream->codecpar->codec_id;
    packetSourceConfigLog(MediaGraphDiagnosticLevel::State, out.str());

    auto emitStatus = emitOutput(context, "codec", buffer.value());
    if (!emitStatus) {
        return emitStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
