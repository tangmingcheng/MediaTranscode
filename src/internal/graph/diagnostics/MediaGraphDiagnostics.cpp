#include "internal/graph/diagnostics/MediaGraphDiagnostics.h"

#include "internal/graph/runtime/buffer/MediaBuffer.h"
#include "internal/graph/runtime/channel/MediaChannel.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <sstream>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>

namespace media::ffmpeg::graph {
namespace {

std::mutex g_diagnosticsMutex;
bool g_diagnosticsEnabled = true;
MediaGraphDiagnosticConfig g_runtimeDiagnosticsConfig;
std::unordered_map<std::string, std::uint64_t> g_sampleCounters;

std::string rationalText(MediaRational rational)
{
    if (!rational.isKnown()) {
        return "unknown";
    }
    return std::to_string(rational.num) + "/" + std::to_string(rational.den);
}

std::string timestampText(int64_t timestamp)
{
    if (timestamp == invalidMediaTimeValue) {
        return "nop";
    }
    return std::to_string(timestamp);
}

std::string lowerCopy(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

bool levelAtLeast(MediaGraphDiagnosticLevel current, MediaGraphDiagnosticLevel required) noexcept
{
    return current != MediaGraphDiagnosticLevel::Off &&
        static_cast<int>(current) >= static_cast<int>(required);
}

} // namespace

const char* mediaGraphDiagnosticPhaseName(MediaGraphDiagnosticPhase phase) noexcept
{
    switch (phase) {
    case MediaGraphDiagnosticPhase::PlannerInput: return "planner.input";
    case MediaGraphDiagnosticPhase::PlannerCapability: return "planner.capability";
    case MediaGraphDiagnosticPhase::PlannerScore: return "planner.score";
    case MediaGraphDiagnosticPhase::PlannerSelect: return "planner.select";
    case MediaGraphDiagnosticPhase::GraphBuild: return "builder.graph";
    case MediaGraphDiagnosticPhase::RuntimeNode: return "runtime.node";
    case MediaGraphDiagnosticPhase::RuntimeEdge: return "runtime.edge";
    case MediaGraphDiagnosticPhase::RuntimeChannel: return "runtime.channel";
    case MediaGraphDiagnosticPhase::RuntimeLifecycle: return "runtime.lifecycle";
    }
    return "unknown";
}

const char* mediaGraphDiagnosticLevelName(MediaGraphDiagnosticLevel level) noexcept
{
    switch (level) {
    case MediaGraphDiagnosticLevel::Off: return "off";
    case MediaGraphDiagnosticLevel::Summary: return "summary";
    case MediaGraphDiagnosticLevel::State: return "state";
    case MediaGraphDiagnosticLevel::Flow: return "flow";
    case MediaGraphDiagnosticLevel::Trace: return "trace";
    }
    return "unknown";
}

MediaGraphDiagnosticLevel mediaGraphDiagnosticLevelFromString(const std::string& text,
                                                              MediaGraphDiagnosticLevel missingLevel) noexcept
{
    const std::string value = lowerCopy(text);
    if (value == "off" || value == "quiet" || value == "none" || value == "0") {
        return MediaGraphDiagnosticLevel::Off;
    }
    if (value == "summary" || value == "1") {
        return MediaGraphDiagnosticLevel::Summary;
    }
    if (value == "state" || value == "2") {
        return MediaGraphDiagnosticLevel::State;
    }
    if (value == "flow" || value == "3") {
        return MediaGraphDiagnosticLevel::Flow;
    }
    if (value == "trace" || value == "4" || value == "all") {
        return MediaGraphDiagnosticLevel::Trace;
    }
    return missingLevel;
}

const char* mediaGraphDiagnosticNodeKindName(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::FileInput: return "FileInput";
    case MediaNodeKind::RealtimeInput: return "RealtimeInput";
    case MediaNodeKind::Demux: return "Demux";
    case MediaNodeKind::MpegTsDemux: return "MpegTsDemux";
    case MediaNodeKind::StreamSplit: return "StreamSplit";
    case MediaNodeKind::PacketFanout: return "PacketFanout";
    case MediaNodeKind::FrameRoute: return "FrameRoute";
    case MediaNodeKind::VideoDecode: return "VideoDecode";
    case MediaNodeKind::VideoTimestamp: return "VideoTimestamp";
    case MediaNodeKind::HardwareTransfer: return "HardwareTransfer";
    case MediaNodeKind::VideoFrameRate: return "VideoFrameRate";
    case MediaNodeKind::VideoFilter: return "VideoFilter";
    case MediaNodeKind::VideoEncode: return "VideoEncode";
    case MediaNodeKind::AudioCodecResolver: return "AudioCodecResolver";
    case MediaNodeKind::AudioDecode: return "AudioDecode";
    case MediaNodeKind::AudioResample: return "AudioResample";
    case MediaNodeKind::AudioEncode: return "AudioEncode";
    case MediaNodeKind::PacketSourceConfig: return "PacketSourceConfig";
    case MediaNodeKind::PacketNormalize: return "PacketNormalize";
    case MediaNodeKind::AvPacketStartBarrier: return "AvPacketStartBarrier";
    case MediaNodeKind::PacketStartGate: return "PacketStartGate";
    case MediaNodeKind::RtpClockGroup: return "RtpClockGroup";
    case MediaNodeKind::RtpPacketClockBinder: return "RtpPacketClockBinder";
    case MediaNodeKind::RtpClockSnapshotFanout: return "RtpClockSnapshotFanout";
    case MediaNodeKind::AvStartupCoordinator: return "AvStartupCoordinator";
    case MediaNodeKind::AvOutputScheduler: return "AvOutputScheduler";
    case MediaNodeKind::PlaybackEpochBinder: return "PlaybackEpochBinder";
    case MediaNodeKind::CanonicalInput: return "CanonicalInput";
    case MediaNodeKind::InitialLockedPacketGate: return "InitialLockedPacketGate";
    case MediaNodeKind::ActivatedStartupReleaseSequencer: return "ActivatedStartupReleaseSequencer";
    case MediaNodeKind::RtpSourceClockStateAdapter: return "RtpSourceClockStateAdapter";
    case MediaNodeKind::AvStartupClock: return "AvStartupClock";
    case MediaNodeKind::SourceClockStateFanout: return "SourceClockStateFanout";
    case MediaNodeKind::AudioDriftController: return "AudioDriftController";
    case MediaNodeKind::EncodedAudioCanonicalizer: return "EncodedAudioCanonicalizer";
    case MediaNodeKind::ScheduledOutputRouter: return "ScheduledOutputRouter";
    case MediaNodeKind::ScheduledRtpSender: return "ScheduledRtpSender";
    case MediaNodeKind::DualMediaSdpPublisher: return "DualMediaSdpPublisher";
    case MediaNodeKind::AvBoundReleaseExtractor: return "AvBoundReleaseExtractor";
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
    default: return "Unknown";
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
    default: return "Unknown";
    }
}

const char* mediaGraphDiagnosticEdgeKindName(MediaEdgeKind kind) noexcept
{
    switch (kind) {
    case MediaEdgeKind::InputPacket: return "InputPacket";
    case MediaEdgeKind::RawFrame: return "RawFrame";
    case MediaEdgeKind::HardwareFrame: return "HardwareFrame";
    case MediaEdgeKind::SoftwareFrame: return "SoftwareFrame";
    case MediaEdgeKind::DecoderConfig: return "DecoderConfig";
    case MediaEdgeKind::EncoderConfig: return "EncoderConfig";
    case MediaEdgeKind::StreamConfig: return "StreamConfig";
    case MediaEdgeKind::EncodedPacket: return "EncodedPacket";
    case MediaEdgeKind::CopiedPacket: return "CopiedPacket";
    case MediaEdgeKind::MuxedPacket: return "MuxedPacket";
    case MediaEdgeKind::Metadata: return "Metadata";
    case MediaEdgeKind::Control: return "Control";
    case MediaEdgeKind::Event: return "Event";
    case MediaEdgeKind::Unknown:
    default: return "Unknown";
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
    default: return "Unknown";
    }
}

void mediaGraphDiagnosticSetGlobalEnabled(bool enabled) noexcept
{
    std::lock_guard lock(g_diagnosticsMutex);
    g_diagnosticsEnabled = enabled;
}

bool mediaGraphDiagnosticGlobalEnabled() noexcept
{
    std::lock_guard lock(g_diagnosticsMutex);
    return g_diagnosticsEnabled;
}

void mediaGraphDiagnosticSetGlobalConfig(MediaGraphDiagnosticConfig config)
{
    std::lock_guard lock(g_diagnosticsMutex);
    g_runtimeDiagnosticsConfig = config;
}

MediaGraphDiagnosticConfig mediaGraphDiagnosticGlobalConfig()
{
    std::lock_guard lock(g_diagnosticsMutex);
    return g_runtimeDiagnosticsConfig;
}

bool mediaGraphDiagnosticLevelEnabled(MediaGraphDiagnosticLevel requiredLevel) noexcept
{
    std::lock_guard lock(g_diagnosticsMutex);
    return g_diagnosticsEnabled && levelAtLeast(g_runtimeDiagnosticsConfig.level, requiredLevel);
}

std::string mediaGraphDiagnosticDescribeBuffer(const MediaBufferRef& buffer)
{
    if (!buffer) {
        return "buffer=null";
    }

    std::ostringstream out;
    out << "buffer=" << buffer.get()
        << " stream=" << mediaGraphDiagnosticStreamKindName(buffer->streamKind())
        << " payload=" << mediaGraphDiagnosticPayloadKindName(buffer->payloadKind())
        << " pts=" << timestampText(buffer->pts())
        << " dts=" << timestampText(buffer->dts())
        << " duration=" << buffer->duration()
        << " tb=" << rationalText(buffer->timeDescriptor().timeBase)
        << " flags=" << static_cast<std::uint32_t>(buffer->flags());
    return out.str();
}

std::string mediaGraphDiagnosticDescribeChannel(const MediaChannel& channel)
{
    std::ostringstream out;
    out << "channel=" << channel.id().value
        << " edge=" << channel.edgeId().value
        << " stream=" << mediaGraphDiagnosticStreamKindName(channel.binding().streamKind)
        << " edge_kind=" << mediaGraphDiagnosticEdgeKindName(channel.binding().edgeKind)
        << " payload=" << mediaGraphDiagnosticPayloadKindName(channel.binding().payloadKind)
        << " queued=" << channel.size();
    return out.str();
}

MediaGraphDiagnosticSampleDecision mediaGraphDiagnosticSample(MediaGraphDiagnosticLevel requiredLevel,
                                                             const std::string& key,
                                                             bool force)
{
    std::lock_guard lock(g_diagnosticsMutex);
    if (!g_diagnosticsEnabled || !levelAtLeast(g_runtimeDiagnosticsConfig.level, requiredLevel)) {
        return {};
    }

    auto& count = g_sampleCounters[key];
    ++count;

    const bool inFirstPacketWindow = count <= g_runtimeDiagnosticsConfig.firstPacketLimit;
    const bool onSampleInterval = g_runtimeDiagnosticsConfig.packetSampleInterval > 0 &&
        (count % g_runtimeDiagnosticsConfig.packetSampleInterval) == 0;
    const bool shouldLog = force || inFirstPacketWindow || onSampleInterval;
    const bool sampled = !force && !inFirstPacketWindow && !onSampleInterval;
    return MediaGraphDiagnosticSampleDecision{ shouldLog, count, sampled };
}

void mediaGraphDiagnosticResetSampling()
{
    std::lock_guard lock(g_diagnosticsMutex);
    g_sampleCounters.clear();
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

void mediaGraphDiagnosticLog(MediaGraphDiagnosticLevel requiredLevel,
                             MediaGraphDiagnosticPhase phase,
                             const std::string& message)
{
    if (!mediaGraphDiagnosticLevelEnabled(requiredLevel)) {
        return;
    }
    spdlog::info("[graph][{}] {}", mediaGraphDiagnosticPhaseName(phase), message);
}

} // namespace media::ffmpeg::graph
