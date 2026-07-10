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

#include <algorithm>
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

::media::Result<MediaNodeProcessResult> PacketNormalizeNode::onProcess(MediaGraphExecutionContext& context)
{
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

    auto packetInput = tryPopInputOptional(context, "packet");
    if (!packetInput) {
        return ::media::Result<MediaNodeProcessResult>::failure(packetInput.error());
    }
    if (!packetInput.value()) {
        return processWaiting();
    }

    MediaBufferRef input = *packetInput.value();
    if (input->isEof() || input->isFlush()) {
        auto status = emitOutput(context, "packet", input);
        return input->isEof() ? processFinished(status) : processProgress(status);
    }

    auto normalized = normalizePacket(input);
    if (!normalized) {
        return ::media::Result<MediaNodeProcessResult>::failure(normalized.error());
    }

    return processProgress(emitOutput(context, "packet", normalized.value()));
}

void PacketNormalizeNode::releaseFormatContext() noexcept
{
    m_formatContextOwner.reset();
    m_sourceStream = nullptr;
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
    if (!formatBuffer || !formatBuffer->inputSnapshotComplete()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode expected FFmpegFormatContextBuffer"));
    }

    m_formatContextOwner = std::move(input);
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

    if (!m_formatContextOwner) {
        return ::media::Status::failure(
            ::media::ErrorInfo::notInitialized("PacketNormalizeNode requires format context before source stream binding"));
    }

    const int index = streamIndex.value();
    const auto* formatBuffer = dynamic_cast<const FFmpegFormatContextBuffer*>(m_formatContextOwner.get());
    m_sourceStream = formatBuffer ? formatBuffer->inputStreamSnapshot(index) : nullptr;
    if (!m_sourceStream || m_sourceStream->streamKind != streamKind.value()) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode planner source stream kind does not match stream"));
    }
    if (m_sourceStream->time.timeBase.num == 0 || m_sourceStream->time.timeBase.den == 0) {
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
    if (!m_sourceStream || m_sourceStreamIndex == invalidMediaStreamIndex) {
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

    MediaFormatDescriptor formatDescriptor = m_sourceStream->format;
    formatDescriptor.streamKind = m_streamKind;
    cloned.value()->setStreamKind(m_streamKind);
    cloned.value()->setPayloadKind(MediaPayloadKind::Packet);
    cloned.value()->setFormatDescriptor(formatDescriptor);
    cloned.value()->setTimeDescriptor(m_sourceStream->time);
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
    if (packet->dts == AV_NOPTS_VALUE && packet->pts == AV_NOPTS_VALUE) {
        return ::media::Status::failure(
            ::media::ErrorInfo::invalidArgument("PacketNormalizeNode monotonic packet timestamps require dts or pts"));
    }

    int64_t shift = 0;
    if (m_nextPacketDts != invalidMediaTimeValue) {
        if (packet->dts != AV_NOPTS_VALUE && packet->dts < m_nextPacketDts) {
            shift = std::max(shift, m_nextPacketDts - packet->dts);
        }
        if (packet->pts != AV_NOPTS_VALUE && packet->pts < m_nextPacketDts) {
            shift = std::max(shift, m_nextPacketDts - packet->pts);
        }
    }
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
        packetNormalizeLog(MediaGraphDiagnosticLevel::State,
                           "monotonic_shift stream=" + std::to_string(m_sourceStreamIndex) +
                               " shift=" + std::to_string(shift));
    }

    int64_t normalizedDts = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;
    if (packet->pts != AV_NOPTS_VALUE && packet->pts > normalizedDts) {
        normalizedDts = packet->pts;
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
