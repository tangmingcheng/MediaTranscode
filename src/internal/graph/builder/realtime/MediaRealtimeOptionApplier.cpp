#include "internal/graph/builder/realtime/MediaRealtimeOptionApplier.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"

namespace media::ffmpeg::graph {
namespace {

constexpr const char* owner = "MediaRealtimeOptionApplier";

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId nodeId,
                                const std::string& key,
                                const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("MediaRealtimeOptionApplier requires non-empty option " + key));
    }
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

::media::Result<void> setOptionalOption(MediaGraph& graph,
                                        MediaNodeId nodeId,
                                        const std::string& key,
                                        const std::string& value)
{
    if (value.empty()) {
        return ::media::Result<void>::success();
    }
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, value);
}

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

} // namespace

::media::Result<void> MediaRealtimeOptionApplier::applyInputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeRtpInputNodePlan& plan)
{
    if (auto status = setOption(graph, nodeId, "url", plan.url); !status) return status;
    if (auto status = setOptionalOption(graph, nodeId, "input.sdp", plan.sdpText); !status) return status;
    if (auto status = setOptionalOption(graph, nodeId, "input.rtsp_transport", plan.rtspTransport); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.open_timeout_ms", std::to_string(plan.openTimeoutMs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.read_timeout_ms", std::to_string(plan.readTimeoutMs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.analyze_duration_us", std::to_string(plan.analyzeDurationUs)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.probe_size_bytes", std::to_string(plan.probeSizeBytes)); !status) return status;
    if (auto status = setOption(graph, nodeId, "input.low_latency", boolOption(plan.lowLatency)); !status) return status;
    if (!plan.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", plan.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applyOutputOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeRtpOutputNodePlan& plan)
{
    if (auto status = setOption(graph, nodeId, "url", plan.url); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.packet_size", std::to_string(plan.packetSize)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.write_pacing.enabled", boolOption(plan.writePacingEnabled)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.write_pacing.bytes_per_second", std::to_string(plan.writePacingBytesPerSecond)); !status) return status;
    if (auto status = setOption(graph, nodeId, "rtp.write_pacing.burst_bytes", std::to_string(plan.writePacingBurstBytes)); !status) return status;
    if (!plan.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", plan.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applySdpWriterOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeSdpWriterPlan& plan)
{
    if (auto status = setOption(graph, nodeId, "path", plan.path); !status) return status;
    if (auto status = setOption(graph, nodeId, "sdp.expected_contexts", std::to_string(plan.expectedContexts)); !status) return status;
    if (!plan.mediaId.empty()) {
        if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "media_id", plan.mediaId); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applyMuxOptions(
    MediaGraph& graph,
    MediaNodeId nodeId,
    const MediaRealtimeMuxNodePlan& plan)
{
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, MediaTranscodeOptionKey::MuxExpectVideo, boolOption(plan.expectVideo)); !status) return status;
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, MediaTranscodeOptionKey::MuxExpectAudio, boolOption(plan.expectAudio)); !status) return status;
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "rtp.pacing.enabled", boolOption(plan.pacingPolicy.enablePacing)); !status) return status;
    if (auto status = MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "rtp.packet_timestamps.monotonic", boolOption(plan.monotonicPacketTimestamps)); !status) return status;
    return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, "rtp.startup_delay_ms", std::to_string(plan.startupDelayMs));
}

} // namespace media::ffmpeg::graph
