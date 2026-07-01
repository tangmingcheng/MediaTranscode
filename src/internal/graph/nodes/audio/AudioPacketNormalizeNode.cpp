#include "internal/graph/nodes/audio/AudioPacketNormalizeNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
}

#include <charconv>
#include <string>

namespace media::ffmpeg::graph {
namespace {

void audioPacketNormalizeLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("audio_packet_normalize.") + message);
}

::media::Result<int> parseRequiredSourceStreamIndex(const MediaNodeOptions* options)
{
    if (!options || !options->has(MediaTranscodeOptionKey::AudioSourceStreamIndex)) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode requires planner option audio.source_stream_index"));
    }

    const std::string value = options->value(MediaTranscodeOptionKey::AudioSourceStreamIndex);
    int streamIndex = invalidMediaStreamIndex;
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, streamIndex);
    if (result.ec != std::errc{} || result.ptr != end || streamIndex < 0) {
        return ::media::Result<int>::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode received invalid audio.source_stream_index"));
    }

    return ::media::Result<int>::success(streamIndex);
}

MediaTimeDescriptor timeDescriptorFromStream(const AVStream* stream)
{
    MediaTimeDescriptor descriptor;
    if (!stream) {
        return descriptor;
    }

    descriptor.timeBase = FFmpegDescriptorMapper::toRational(stream->time_base);
    descriptor.frameRate = FFmpegDescriptorMapper::toRational(stream->avg_frame_rate);
    descriptor.startTime = stream->start_time;
    descriptor.duration = stream->duration;
    return descriptor;
}

} // namespace

AudioPacketNormalizeNode::AudioPacketNormalizeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "AudioPacketNormalizeNode")
{
}

MediaNodeKind AudioPacketNormalizeNode::staticKind() noexcept
{
    return MediaNodeKind::AudioPacketNormalize;
}

::media::Status AudioPacketNormalizeNode::onProcess(MediaGraphExecutionContext& context)
{
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

    auto packetInput = tryPopInputOptional(context, "packet");
    if (!packetInput) {
        return ::media::Status::failure(packetInput.error());
    }
    if (!packetInput.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef input = *packetInput.value();
    if (input->isEof() || input->isFlush()) {
        return emitOutput(context, "packet", input);
    }

    auto normalized = normalizePacket(input);
    if (!normalized) {
        return ::media::Status::failure(normalized.error());
    }

    return emitOutput(context, "packet", normalized.value());
}

::media::Status AudioPacketNormalizeNode::bindFormatContext(MediaGraphExecutionContext& context)
{
    auto formatInput = tryPopInputOptional(context, "format");
    if (!formatInput) {
        return ::media::Status::failure(formatInput.error());
    }
    if (!formatInput.value()) {
        return ::media::Status::success();
    }

    MediaBufferRef input = *formatInput.value();
    auto* formatBuffer = dynamic_cast<FFmpegFormatContextBuffer*>(input.get());
    if (!formatBuffer || !formatBuffer->context()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(input);
    m_formatContext = formatBuffer->context();
    audioPacketNormalizeLog(MediaGraphDiagnosticLevel::State, "bind_format_context");
    return ::media::Status::success();
}

::media::Status AudioPacketNormalizeNode::bindSourceStreamIndex(MediaGraphExecutionContext& context)
{
    auto streamIndex = parseRequiredSourceStreamIndex(nodeOptions(context));
    if (!streamIndex) {
        return ::media::Status::failure(streamIndex.error());
    }

    if (!m_formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("AudioPacketNormalizeNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    if (index < 0 || index >= static_cast<int>(m_formatContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode planner audio source stream index is out of range"));
    }

    const AVStream* stream = m_formatContext->streams[index];
    if (!stream || !stream->codecpar || stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode planner audio source stream is not an audio stream"));
    }

    if (stream->time_base.num == 0 || stream->time_base.den == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode requires known upstream audio time_base"));
    }

    m_sourceStreamIndex = index;
    audioPacketNormalizeLog(MediaGraphDiagnosticLevel::State,
                            std::string("bind_source_stream index=") + std::to_string(m_sourceStreamIndex));
    return ::media::Status::success();
}

::media::Result<MediaBufferRef> AudioPacketNormalizeNode::normalizePacket(const MediaBufferRef& buffer)
{
    if (!m_formatContext || m_sourceStreamIndex == invalidMediaStreamIndex) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("AudioPacketNormalizeNode requires source stream before packet normalization"));
    }

    const AVPacket* sourcePacket = FFmpegPacketView::packet(buffer);
    if (!sourcePacket) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode expected packet buffer"));
    }

    if (sourcePacket->stream_index != m_sourceStreamIndex) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode received packet from non-planned audio stream"));
    }

    AVStream* sourceStream = m_formatContext->streams[m_sourceStreamIndex];
    if (!sourceStream) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("AudioPacketNormalizeNode source stream is null"));
    }

    auto cloned = FFmpegBufferFactory::clonePacket(sourcePacket, MediaStreamKind::Audio);
    if (!cloned) {
        return cloned;
    }

    AVPacket* packet = FFmpegPacketView::writablePacket(cloned.value());
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("AudioPacketNormalizeNode cloned packet is not writable"));
    }

    packet->pos = -1;

    MediaFormatDescriptor formatDescriptor = FFmpegDescriptorMapper::fromStream(sourceStream);
    cloned.value()->setStreamKind(MediaStreamKind::Audio);
    cloned.value()->setPayloadKind(MediaPayloadKind::Packet);
    cloned.value()->setFormatDescriptor(formatDescriptor);
    cloned.value()->setTimeDescriptor(timeDescriptorFromStream(sourceStream));
    cloned.value()->setTimestamps(packet->pts, packet->dts, packet->duration);

    return cloned;
}

} // namespace media::ffmpeg::graph
