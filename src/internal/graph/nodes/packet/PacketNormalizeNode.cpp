#include "internal/graph/nodes/packet/PacketNormalizeNode.h"

#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/buffer/FFmpegFormatContextBuffer.h"
#include "internal/graph/runtime/ffmpeg/FFmpegBufferFactory.h"
#include "internal/graph/runtime/ffmpeg/FFmpegDescriptorMapper.h"
#include "internal/graph/runtime/ffmpeg/FFmpegPacketView.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <limits>
#include <string>

namespace media::ffmpeg::graph {
namespace {

void packetNormalizeLog(MediaGraphDiagnosticLevel level, const std::string& message)
{
    mediaGraphDiagnosticLog(level,
                            MediaGraphDiagnosticPhase::RuntimeNode,
                            std::string("packet_normalize.") + message);
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

PacketNormalizeNode::PacketNormalizeNode(MediaNodeId nodeId)
    : FFmpegNodeRuntime(nodeId, staticKind(), "PacketNormalizeNode")
{
}

MediaNodeKind PacketNormalizeNode::staticKind() noexcept
{
    return MediaNodeKind::PacketNormalize;
}

::media::Status PacketNormalizeNode::stop(MediaGraphExecutionContext& context)
{
    releaseFormatContext();
    return FFmpegNodeRuntime::stop(context);
}

void PacketNormalizeNode::abort(MediaGraphExecutionContext& context) noexcept
{
    releaseFormatContext();
    FFmpegNodeRuntime::abort(context);
}

::media::Status PacketNormalizeNode::onProcess(MediaGraphExecutionContext& context)
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
        auto streamStatus = bindSourceStream(context);
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

void PacketNormalizeNode::releaseFormatContext() noexcept
{
    m_formatContextOwner.reset();
    m_formatContext = nullptr;
    m_streamKind = MediaStreamKind::Unknown;
    m_sourceStreamIndex = invalidMediaStreamIndex;
    m_monotonicPacketTimestamps = false;
    m_nextPacketDts = invalidMediaTimeValue;
}

::media::Status PacketNormalizeNode::bindFormatContext(MediaGraphExecutionContext& context)
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
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(input);
    m_formatContext = formatBuffer->context();
    packetNormalizeLog(MediaGraphDiagnosticLevel::State, "bind_format_context");
    return ::media::Status::success();
}

::media::Status PacketNormalizeNode::bindSourceStream(MediaGraphExecutionContext& context)
{
    auto streamKind = requiredStreamKindNodeOption(nodeOptions(context),
                                                   "PacketNormalizeNode",
                                                   MediaTranscodeOptionKey::PacketStreamKind);
    if (!streamKind) {
        return ::media::Status::failure(streamKind.error());
    }
    auto streamIndex = requiredNonNegativeIntNodeOption(nodeOptions(context),
                                                        "PacketNormalizeNode",
                                                        MediaTranscodeOptionKey::PacketSourceStreamIndex);
    if (!streamIndex) {
        return ::media::Status::failure(streamIndex.error());
    }

    if (!m_formatContext) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketNormalizeNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    if (index < 0 || index >= static_cast<int>(m_formatContext->nb_streams)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode planner source stream index is out of range"));
    }

    const AVStream* stream = m_formatContext->streams[index];
    if (!streamTypeMatches(streamKind.value(), stream)) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode planner source stream kind does not match stream"));
    }
    if (stream->time_base.num == 0 || stream->time_base.den == 0) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode requires known upstream packet time_base"));
    }
    auto monotonicPacketTimestamps = requiredBoolNodeOption(nodeOptions(context),
                                                           "PacketNormalizeNode",
                                                           MediaTranscodeOptionKey::PacketMonotonicTimestamps);
    if (!monotonicPacketTimestamps) {
        return ::media::Status::failure(monotonicPacketTimestamps.error());
    }

    m_streamKind = streamKind.value();
    m_sourceStreamIndex = index;
    m_monotonicPacketTimestamps = monotonicPacketTimestamps.value();
    packetNormalizeLog(MediaGraphDiagnosticLevel::State,
                       std::string("bind_source_stream index=") + std::to_string(m_sourceStreamIndex));
    return ::media::Status::success();
}

::media::Result<MediaBufferRef> PacketNormalizeNode::normalizePacket(const MediaBufferRef& buffer)
{
    if (!m_formatContext || m_sourceStreamIndex == invalidMediaStreamIndex) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("PacketNormalizeNode requires source stream before packet normalization"));
    }

    const AVPacket* sourcePacket = FFmpegPacketView::packet(buffer);
    if (!sourcePacket) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode expected packet buffer"));
    }

    if (sourcePacket->stream_index != m_sourceStreamIndex) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode received packet from non-planned stream"));
    }

    AVStream* sourceStream = m_formatContext->streams[m_sourceStreamIndex];
    if (!sourceStream) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::notInitialized("PacketNormalizeNode source stream is null"));
    }

    auto cloned = FFmpegBufferFactory::clonePacket(sourcePacket, m_streamKind);
    if (!cloned) {
        return cloned;
    }

    AVPacket* packet = FFmpegPacketView::writablePacket(cloned.value());
    if (!packet) {
        return ::media::Result<MediaBufferRef>::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode cloned packet is not writable"));
    }

    packet->pos = -1;

    MediaFormatDescriptor formatDescriptor = FFmpegDescriptorMapper::fromStream(sourceStream);
    formatDescriptor.streamKind = m_streamKind;
    cloned.value()->setStreamKind(m_streamKind);
    cloned.value()->setPayloadKind(MediaPayloadKind::Packet);
    cloned.value()->setFormatDescriptor(formatDescriptor);
    cloned.value()->setTimeDescriptor(timeDescriptorFromStream(sourceStream));
    cloned.value()->setTimestamps(packet->pts, packet->dts, packet->duration);

    if (auto status = normalizePacketTimestamps(cloned.value()); !status) {
        return ::media::Result<MediaBufferRef>::failure(status.error());
    }

    return cloned;
}

::media::Status PacketNormalizeNode::normalizePacketTimestamps(MediaBufferRef& buffer)
{
    if (!m_monotonicPacketTimestamps) {
        return ::media::Status::success();
    }

    AVPacket* packet = FFmpegPacketView::writablePacket(buffer);
    if (!packet) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode expected writable packet for timestamp normalization"));
    }
    const int64_t packetDts = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;
    if (packetDts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode monotonic packet timestamps require dts or pts"));
    }

    int64_t normalizedDts = packetDts;
    if (m_nextPacketDts != invalidMediaTimeValue && normalizedDts < m_nextPacketDts) {
        normalizedDts = m_nextPacketDts;
    }
    const int64_t shift = normalizedDts - packetDts;
    if (shift > 0) {
        if (packet->pts != AV_NOPTS_VALUE) {
            if (packet->pts > std::numeric_limits<int64_t>::max() - shift) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument("PacketNormalizeNode packet pts overflow"));
            }
            packet->pts += shift;
        }
        if (packet->dts != AV_NOPTS_VALUE) {
            if (packet->dts > std::numeric_limits<int64_t>::max() - shift) {
                return ::media::Status::failure(
                    ::media::ErrorInfo::invalidArgument("PacketNormalizeNode packet dts overflow"));
            }
            packet->dts += shift;
        }
    }

    const int64_t duration = packet->duration > 0 ? packet->duration : 1;
    if (normalizedDts > std::numeric_limits<int64_t>::max() - duration) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode packet timestamp cannot advance past int64 max"));
    }
    m_nextPacketDts = normalizedDts + duration;
    buffer->setTimestamps(packet->pts, packet->dts, packet->duration);
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
