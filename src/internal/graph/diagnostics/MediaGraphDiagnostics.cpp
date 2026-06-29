#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

#include <atomic>
#include <sstream>
#include <spdlog/spdlog.h>

namespace media::ffmpeg::graph {
namespace {

std::atomic_bool g_runtimeDiagnosticsEnabled{ false };

std::string rationalText(MediaRational rational)
{
    if (!rational.isKnown()) {
        return "unknown";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

} // namespace

const char* mediaGraphDiagnosticPhaseName(MediaGraphDiagnosticPhase phase) noexcept
{
    switch (phase) {
    case MediaGraphDiagnosticPhase::PlannerInput:
        return "planner.input";
    case MediaGraphDiagnosticPhase::PlannerCapability:
        return "planner.capability";
    case MediaGraphDiagnosticPhase::PlannerScore:
        return "planner.score";
    case MediaGraphDiagnosticPhase::PlannerSelect:
        return "planner.select";
    case MediaGraphDiagnosticPhase::GraphBuild:
        return "builder.graph";
    case MediaGraphDiagnosticPhase::RuntimeNode:
        return "runtime.node";
    case MediaGraphDiagnosticPhase::RuntimeEdge:
        return "runtime.edge";
    case MediaGraphDiagnosticPhase::RuntimeChannel:
        return "runtime.channel";
    case MediaGraphDiagnosticPhase::RuntimeLifecycle:
        return "runtime.lifecycle";
    }
    return "unknown";
}

const char* mediaGraphDiagnosticNodeKindName(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::FileInput: return "FileInput";
    case MediaNodeKind::RealtimeInput: return "RealtimeInput";
    case MediaNodeKind::RealtimePacketSource: return "RealtimePacketSource";
    case MediaNodeKind::Demux: return "Demux";
    case MediaNodeKind::StreamSplit: return "StreamSplit";
    case MediaNodeKind::PacketFanout: return "PacketFanout";
    case MediaNodeKind::FrameRoute: return "FrameRoute";
    case MediaNodeKind::VideoDecode: return "VideoDecode";
    case MediaNodeKind::VideoTimestamp: return "VideoTimestamp";
    case MediaNodeKind::HardwareTransfer: return "HardwareTransfer";
    case MediaNodeKind::VideoFrameRate: return "VideoFrameRate";
    case MediaNodeKind::VideoFilter: return "VideoFilter";
    case MediaNodeKind::VideoEncode: return "VideoEncode";
    case MediaNodeKind::VideoPacketDrain: return "VideoPacketDrain";
    case MediaNodeKind::AudioStrategy: return "AudioStrategy";
    case MediaNodeKind::AudioCopy: return "AudioCopy";
    case MediaNodeKind::AudioDecode: return "AudioDecode";
    case MediaNodeKind::AudioResample: return "AudioResample";
    case MediaNodeKind::AudioEncode: return "AudioEncode";
    case MediaNodeKind::AudioPacketNormalize: return "AudioPacketNormalize";
    case MediaNodeKind::AudioPacketDrain: return "AudioPacketDrain";
    case MediaNodeKind::PacketMerge: return "PacketMerge";
    case MediaNodeKind::FileMux: return "FileMux";
    case MediaNodeKind::RtpMux: return "RtpMux";
    case MediaNodeKind::FileOutput: return "FileOutput";
    case MediaNodeKind::RtpOutput: return "RtpOutput";
    case MediaNodeKind::SdpWriter: return "SdpWriter";
    case MediaNodeKind::EofBarrier: return "EofBarrier";
    case MediaNodeKind::Flush: return "Flush";
    case MediaNodeKind::Finalize: return "Finalize";
    case MediaNodeKind::ControlSignal: return "ControlSignal";
    case MediaNodeKind::MetadataProbe: return "MetadataProbe";
    case MediaNodeKind::DebugDump: return "DebugDump";
    case MediaNodeKind::TraceProbe: return "TraceProbe";
    case MediaNodeKind::CodecResolver: return "CodecResolver";
    case MediaNodeKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* mediaGraphDiagnosticStreamKindName(MediaStreamKind kind) noexcept
{
    switch (kind) {
    case MediaStreamKind::Video: return "Video";
    case MediaStreamKind::Audio: return "Audio";
    case MediaStreamKind::Subtitle: return "Subtitle";
    case MediaStreamKind::Data: return "Data";
    case MediaStreamKind::Attachment: return "Attachment";
    case MediaStreamKind::Control: return "Control";
    case MediaStreamKind::Metadata: return "Metadata";
    case MediaStreamKind::Any: return "Any";
    case MediaStreamKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* mediaGraphDiagnosticEdgeKindName(MediaEdgeKind kind) noexcept
{
    switch (kind) {
    case MediaEdgeKind::InputPacket: return "InputPacket";
    case MediaEdgeKind::RawFrame: return "RawFrame";
    case MediaEdgeKind::HardwareFrame: return "HardwareFrame";
    case MediaEdgeKind::SoftwareFrame: return "SoftwareFrame";
    case MediaEdgeKind::EncodedPacket: return "EncodedPacket";
    case MediaEdgeKind::CopiedPacket: return "CopiedPacket";
    case MediaEdgeKind::MuxedPacket: return "MuxedPacket";
    case MediaEdgeKind::Metadata: return "Metadata";
    case MediaEdgeKind::Control: return "Control";
    case MediaEdgeKind::Event: return "Event";
    case MediaEdgeKind::Unknown:
    default:
        return "Unknown";
    }
}

const char* mediaGraphDiagnosticPayloadKindName(MediaPayloadKind kind) noexcept
{
    switch (kind) {
    case MediaPayloadKind::FormatContext: return "FormatContext";
    case MediaPayloadKind::StreamDescriptor: return "StreamDescriptor";
    case MediaPayloadKind::CodecContext: return "CodecContext";
    case MediaPayloadKind::CodecParameters: return "CodecParameters";
    case MediaPayloadKind::Packet: return "Packet";
    case MediaPayloadKind::Frame: return "Frame";
    case MediaPayloadKind::TimeDescriptor: return "TimeDescriptor";
    case MediaPayloadKind::HardwareDescriptor: return "HardwareDescriptor";
    case MediaPayloadKind::AudioLayoutDescriptor: return "AudioLayoutDescriptor";
    case MediaPayloadKind::VideoFormatDescriptor: return "VideoFormatDescriptor";
    case MediaPayloadKind::ControlSignal: return "ControlSignal";
    case MediaPayloadKind::GraphEvent: return "GraphEvent";
    case MediaPayloadKind::DiagnosticRecord: return "DiagnosticRecord";
    case MediaPayloadKind::Unknown:
    default:
        return "Unknown";
    }
}

void mediaGraphDiagnosticSetGlobalEnabled(bool enabled) noexcept
{
    g_runtimeDiagnosticsEnabled.store(enabled, std::memory_order_relaxed);
}

bool mediaGraphDiagnosticGlobalEnabled() noexcept
{
    return g_runtimeDiagnosticsEnabled.load(std::memory_order_relaxed);
}

std::string mediaGraphDiagnosticDescribeBuffer(const MediaBufferRef& buffer)
{
    if (!buffer) {
        return "buffer=null";
    }

    std::ostringstream out;
    out << "buffer_type=";
    switch (buffer->type()) {
    case MediaBufferType::FormatContext: out << "FormatContext"; break;
    case MediaBufferType::CodecContext: out << "CodecContext"; break;
    case MediaBufferType::CodecParameters: out << "CodecParameters"; break;
    case MediaBufferType::Packet: out << "Packet"; break;
    case MediaBufferType::Frame: out << "Frame"; break;
    case MediaBufferType::HardwareFrame: out << "HardwareFrame"; break;
    case MediaBufferType::Control: out << "Control"; break;
    case MediaBufferType::Event: out << "Event"; break;
    case MediaBufferType::Unknown:
    default: out << "Unknown"; break;
    }

    out << " stream=" << mediaGraphDiagnosticStreamKindName(buffer->streamKind())
        << " payload=" << mediaGraphDiagnosticPayloadKindName(buffer->payloadKind())
        << " pts=" << buffer->pts()
        << " dts=" << buffer->dts()
        << " duration=" << buffer->duration()
        << " flags=";

    bool anyFlag = false;
    auto appendFlag = [&](const char* name, MediaBufferFlag flag) {
        if (!hasFlag(buffer->flags(), flag)) {
            return;
        }
        if (anyFlag) {
            out << "+";
        }
        out << name;
        anyFlag = true;
    };
    appendFlag("eof", MediaBufferFlag::Eof);
    appendFlag("flush", MediaBufferFlag::Flush);
    appendFlag("key", MediaBufferFlag::KeyFrame);
    appendFlag("discontinuity", MediaBufferFlag::Discontinuity);
    appendFlag("corrupt", MediaBufferFlag::Corrupt);
    appendFlag("dropped", MediaBufferFlag::Dropped);
    appendFlag("hw", MediaBufferFlag::HardwareBacked);
    if (!anyFlag) {
        out << "none";
    }

    const auto& format = buffer->formatDescriptor();
    out << " format_stream=" << mediaGraphDiagnosticStreamKindName(format.streamKind)
        << " stream_index=" << format.streamIndex
        << " codec=" << (format.codec.codecName.empty() ? "unknown" : format.codec.codecName)
        << " size=" << format.video.size.width << "x" << format.video.size.height
        << " sample_rate=" << format.audio.sampleRate
        << " channels=" << format.audio.channels
        << " tb=" << rationalText(buffer->timeDescriptor().timeBase)
        << " fr=" << rationalText(buffer->timeDescriptor().frameRate);
    return out.str();
}

std::string mediaGraphDiagnosticDescribeChannel(const MediaChannel& channel)
{
    const auto& binding = channel.binding();
    std::ostringstream out;
    out << "channel=" << channel.id().value
        << " edge=" << channel.edgeId().value
        << " from=" << binding.from.nodeId.value << ":" << binding.from.portId.value
        << " to=" << binding.to.nodeId.value << ":" << binding.to.portId.value
        << " stream=" << mediaGraphDiagnosticStreamKindName(binding.streamKind)
        << " edge_kind=" << mediaGraphDiagnosticEdgeKindName(binding.edgeKind)
        << " payload=" << mediaGraphDiagnosticPayloadKindName(binding.payloadKind)
        << " queue=" << channel.size() << "/" << channel.capacity();
    return out.str();
}

void mediaGraphDiagnosticLog(bool enabled,
                             MediaGraphDiagnosticPhase phase,
                             const std::string& message)
{
    if (!enabled) {
        return;
    }

    spdlog::info("[graph][{}] {}", mediaGraphDiagnosticPhaseName(phase), message);
}

} // namespace media::ffmpeg::graph
