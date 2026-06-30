#include "internal/graph/nodes/audio/AudioSourceConfigNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"

extern "C" {
#include <libavformat/avformat.h>
}

#include <charconv>
#include <sstream>
#include <string>

namespace media::ffmpeg::graph {
namespace {

constexpr const char* kAudioSourceStreamIndexOption = "audio.source_stream_index";

void audioSourceConfigLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("audio_source_config.") + message);
}

::media::Result<int> parseRequiredSourceStreamIndex(const MediaNodeOptions* options)
{
    if (!options || !options->has(kAudioSourceStreamIndexOption)) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("AudioSourceConfigNode requires planner option audio.source_stream_index"));
    }

    const std::string value = options->value(kAudioSourceStreamIndexOption);
    int streamIndex = invalidMediaStreamIndex;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, streamIndex);
    if (result.ec != std::errc{} || result.ptr != end || streamIndex < 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("AudioSourceConfigNode received invalid audio.source_stream_index"));
    }

    return ::media::Result<int>::success(streamIndex);
}

} // namespace

AudioSourceConfigNode::AudioSourceConfigNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioSourceConfigNode")
{
}

MediaNodeKind AudioSourceConfigNode::staticKind() noexcept
{
    return MediaNodeKind::AudioSourceConfig;
}

::media::Status AudioSourceConfigNode::onProcess(MediaGraphExecutionContext& context)
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
        auto streamStatus = bindSourceStreamIndex(context);
        if (!streamStatus) {
            return streamStatus;
        }
    }

    return emitSourceConfig(context);
}

::media::Status AudioSourceConfigNode::bindFormatContext(MediaGraphExecutionContext& context)
{
    auto input = tryPopFirstInput(context);
    if (!input) {
        return ::media::Status::success();
    }

    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.value().get());
    if (!formatBuffer || !formatBuffer->context()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioSourceConfigNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(input).value();
    m_formatContext = formatBuffer->context();
    audioSourceConfigLog(MediaGraphDiagnosticLevel::State, "bind_format_context");
    return ::media::Status::success();
}

::media::Status AudioSourceConfigNode::bindSourceStreamIndex(MediaGraphExecutionContext& context)
{
    auto streamIndex = parseRequiredSourceStreamIndex(nodeOptions(context));
    if (!streamIndex) {
        return ::media::Status::failure(streamIndex.error());
    }

    if (!m_formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("AudioSourceConfigNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    if (index < 0 || index >= static_cast<int>(m_formatContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioSourceConfigNode planner audio source stream index is out of range"));
    }

    const AVStream* stream = m_formatContext->streams[index];
    if (!stream || !stream->codecpar || stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioSourceConfigNode planner audio source stream is not an audio stream"));
    }

    m_sourceStreamIndex = index;
    audioSourceConfigLog(MediaGraphDiagnosticLevel::State,
                         std::string("bind_source_stream index=") + std::to_string(m_sourceStreamIndex));
    return ::media::Status::success();
}

::media::Status AudioSourceConfigNode::emitSourceConfig(MediaGraphExecutionContext& context)
{
    if (!m_formatContext || m_sourceStreamIndex == invalidMediaStreamIndex) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("AudioSourceConfigNode requires source stream before emitting config"));
    }

    AVStream* stream = m_formatContext->streams[m_sourceStreamIndex];
    auto buffer = FFmpegBufferFactory::cloneCodecParameters(stream);
    if (!buffer) {
        return ::media::Status::failure(buffer.error());
    }

    std::ostringstream out;
    out << "emit_source_config stream_index=" << m_sourceStreamIndex
        << " time_base=" << stream->time_base.num << "/" << stream->time_base.den
        << " codec_id=" << stream->codecpar->codec_id;
    audioSourceConfigLog(MediaGraphDiagnosticLevel::State, out.str());

    auto emitStatus = emitOutput(context, "codec", buffer.value());
    if (!emitStatus) {
        return emitStatus;
    }

    m_emitted = true;
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
