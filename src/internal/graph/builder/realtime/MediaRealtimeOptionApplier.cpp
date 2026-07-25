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
    if (plan.rtpTransport.has_value() != plan.rtpDepacketizer.has_value()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument("raw RTP input requires transport and depacketizer plans together"));
    }
    if (plan.rtpTransport && plan.rtpDepacketizer) {
        const auto& transport = *plan.rtpTransport;
        const auto& depacketizer = *plan.rtpDepacketizer;
        const auto set = [&](const char* key, std::string value) {
            return MediaGraphBuildSupport::setNodeOptionChecked(graph, owner, nodeId, key, std::move(value));
        };
        if (auto status = set("rtp.address_family", transport.addressFamily == MediaIpAddressFamily::Ipv6 ? "ipv6" : "ipv4"); !status) return status;
        if (auto status = set("rtp.bind_address", transport.bindAddress); !status) return status;
        if (auto status = set("rtp.port", std::to_string(transport.rtpPort)); !status) return status;
        if (auto status = set("rtcp.port", std::to_string(transport.rtcpPort)); !status) return status;
        if (auto status = set("rtp.payload_type", std::to_string(transport.payloadType)); !status) return status;
        if (auto status = set("rtp.clock_rate", std::to_string(transport.clockRate)); !status) return status;
        if (auto status = set("rtp.receive_buffer_bytes", std::to_string(transport.receiveBufferBytes)); !status) return status;
        if (auto status = set("rtp.maximum_datagram_bytes", std::to_string(transport.maximumDatagramBytes)); !status) return status;
        if (auto status = set("rtp.reorder_window_packets", std::to_string(transport.reorderWindowPackets)); !status) return status;
        if (auto status = set("rtp.maximum_reorder_delay_ms", std::to_string(transport.maximumReorderDelayMs)); !status) return status;
        if (auto status = set("rtp.cancellable_read_timeout_ms", std::to_string(transport.cancellableReadTimeoutMs)); !status) return status;
        if (auto status = set("rtcp.require_sender_reports", boolOption(transport.requireSenderReports)); !status) return status;
        if (auto status = set("rtcp.require_cname", boolOption(transport.requireCname)); !status) return status;
        if (auto status = set("rtcp.sender_report_timeout_ms", std::to_string(transport.senderReportTimeoutMs)); !status) return status;
        if (auto status = set("rtcp.cname_timeout_ms", std::to_string(transport.cnameTimeoutMs)); !status) return status;
        if (!transport.rtcpCompositionMode) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "raw RTP input requires planner-owned RTCP composition mode"));
        }
        auto composition = serializeMediaRtcpCompositionMode(
            *transport.rtcpCompositionMode);
        if (!composition) return ::media::Result<void>::failure(composition.error());
        if (auto status = set("rtcp.composition_mode", std::move(composition).value()); !status) return status;
        if (auto status = set("rtp.stream_kind", depacketizer.streamKind == MediaStreamKind::Video ? "video" : "audio"); !status) return status;
        if (auto status = set("rtp.codec", depacketizer.codecName); !status) return status;
        if (auto status = set("rtp.fmtp", depacketizer.fmtp); !status) return status;
        if (auto status = set("rtp.channels", std::to_string(depacketizer.channels)); !status) return status;
        if (auto status = set("rtp.access_unit_duration_ticks", std::to_string(depacketizer.accessUnitDurationRtpTicks)); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<void> MediaRealtimeOptionApplier::applyMpegTsDemuxOptions(
    MediaGraph& graph, MediaNodeId nodeId, const MediaRealtimeTsInputPlan& plan)
{
    const auto set = [&](const char* key, auto value) {
        return MediaGraphBuildSupport::setNodeOptionChecked(
            graph, owner, nodeId, key, std::to_string(value));
    };
    if (auto s = set("mpegts.program_number", plan.programNumber); !s) return s;
    if (auto s = set("mpegts.pmt_pid", plan.programMapPid); !s) return s;
    if (auto s = set("mpegts.video_pid", plan.videoPid); !s) return s;
    if (auto s = set("mpegts.audio_pid", plan.audioPid); !s) return s;
    if (auto s = set("mpegts.pcr_pid", plan.pcrPid); !s) return s;
    if (auto s = set("mpegts.pcr_interval_27mhz", plan.pcrInterval27Mhz); !s) return s;
    if (auto s = set("mpegts.maximum_pcr_jitter_27mhz", plan.maximumPcrJitter27Mhz); !s) return s;
    if (auto s = set("mpegts.maximum_pcr_gap_27mhz", plan.maximumPcrGap27Mhz); !s) return s;
    if (auto s = set("mpegts.packet_stride", plan.packetSize); !s) return s;
    if (auto s = set("mpegts.evidence_timeline_capacity", plan.evidenceTimelineCapacity); !s) return s;
    if (auto s = set("mpegts.projection_capacity", plan.projectionCapacity); !s) return s;
    if (auto s = set("mpegts.initial_acquiring_video_packet_capacity",
                     plan.initialAcquiringVideoPacketCapacity); !s) return s;
    if (auto s = set("mpegts.initial_acquiring_audio_packet_capacity",
                     plan.initialAcquiringAudioPacketCapacity); !s) return s;
    if (auto s = set("mpegts.initial_acquiring_video_byte_capacity",
                     plan.initialAcquiringVideoByteCapacity); !s) return s;
    if (auto s = set("mpegts.initial_acquiring_audio_byte_capacity",
                     plan.initialAcquiringAudioByteCapacity); !s) return s;
    if (auto s = set("mpegts.maximum_acquiring_video_packet_bytes",
                     plan.maximumAcquiringVideoPacketBytes); !s) return s;
    if (auto s = set("mpegts.maximum_acquiring_audio_packet_bytes",
                     plan.maximumAcquiringAudioPacketBytes); !s) return s;
    if (auto s = set("mpegts.maximum_position_regression_bytes", plan.maximumPacketPositionRegressionBytes); !s) return s;
    if (auto s = set("mpegts.pes_provenance_capacity", plan.pesProvenanceCapacity); !s) return s;
    if (auto s = set("mpegts.packet_origin_policy",
                     static_cast<int>(plan.packetOriginPolicy)); !s) return s;
    if (auto s = set("mpegts.timestamp_time_base_num", plan.timestampTimeBaseNumerator); !s) return s;
    if (auto s = set("mpegts.timestamp_time_base_den", plan.timestampTimeBaseDenominator); !s) return s;
    if (auto s = set("mpegts.initial_source_generation", plan.initialSourceGeneration); !s) return s;
    return set("mpegts.initial_raw_generation", plan.initialRawTransportGeneration);
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
