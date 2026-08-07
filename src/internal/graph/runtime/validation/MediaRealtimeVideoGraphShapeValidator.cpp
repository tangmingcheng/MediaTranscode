#include "internal/graph/runtime/validation/MediaRealtimeVideoGraphShapeValidator.h"

#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/model/MediaTranscodeParameters.h"
#include "internal/graph/nodes/MediaRequiredNodeOptions.h"
#include "internal/graph/runtime/factory/MediaRealtimeRuntimeBinding.h"
#include "internal/graph/runtime/validation/MediaAvSyncGraphShape.h"

#include <string>

namespace media::ffmpeg::graph {
namespace {

bool audioOrAvNode(MediaNodeKind kind) noexcept
{
    switch (kind) {
    case MediaNodeKind::AudioCodecResolver:
    case MediaNodeKind::AudioDecode:
    case MediaNodeKind::AudioStartupTrim:
    case MediaNodeKind::AudioResample:
    case MediaNodeKind::AudioEncode:
    case MediaNodeKind::RtpClockGroup:
    case MediaNodeKind::RtpPacketClockBinder:
    case MediaNodeKind::DemuxPacketClockBinder:
    case MediaNodeKind::RtpClockSnapshotFanout:
    case MediaNodeKind::AvStartupCoordinator:
    case MediaNodeKind::AvOutputScheduler:
    case MediaNodeKind::PlaybackEpochBinder:
    case MediaNodeKind::CanonicalInput:
    case MediaNodeKind::LockedPacketGate:
    case MediaNodeKind::AvBoundReleaseExtractor:
    case MediaNodeKind::ActivatedStartupReleaseSequencer:
    case MediaNodeKind::RtpSourceClockStateAdapter:
    case MediaNodeKind::AvStartupClock:
    case MediaNodeKind::SourceClockStateFanout:
    case MediaNodeKind::AudioDriftController:
    case MediaNodeKind::EncodedAudioCanonicalizer:
    case MediaNodeKind::ScheduledOutputRouter:
        return true;
    default:
        return false;
    }
}

const char* boolOption(bool value) noexcept
{
    return value ? "1" : "0";
}

bool optionEquals(
    const MediaNode& node,
    const char* key,
    const std::string& expected)
{
    return node.options.has(key) && node.options.value(key) == expected;
}

bool optionalOptionEquals(
    const MediaNode& node,
    const char* key,
    const std::string& expected)
{
    return expected.empty()
        ? !node.options.has(key)
        : optionEquals(node, key, expected);
}

bool hasExactSchedulerEdges(
    const MediaGraph& graph,
    const MediaNode& scheduler,
    const MediaNode& mux,
    const MediaRealtimeVideoRuntimePlan& runtime)
{
    const MediaPort* input = scheduler.findInputPort("video");
    const MediaPort* output = scheduler.findOutputPort("scheduled_video");
    const MediaPort* muxPacket = mux.findInputPort("packet");
    if (!input || !output || !muxPacket) return false;
    const MediaEdge* inputEdge = nullptr;
    const MediaEdge* outputEdge = nullptr;
    for (const MediaEdge& edge : graph.edges()) {
        if (edge.to.portId == input->id) {
            if (inputEdge) return false;
            inputEdge = &edge;
        }
        if (edge.from.portId == output->id) {
            if (outputEdge) return false;
            outputEdge = &edge;
        }
    }
    return inputEdge && outputEdge &&
        inputEdge->policy == runtime.edgePolicies.synchronizedPacket &&
        outputEdge->policy == runtime.edgePolicies.videoMux &&
        outputEdge->to.portId == muxPacket->id;
}

bool matchesRtpOutput(
    const MediaNode& node,
    const MediaRealtimeRtpOutputNodePlan& output)
{
    return optionEquals(node, "url", output.url) &&
        optionEquals(node, "rtp.packet_size", std::to_string(output.packetSize)) &&
        optionEquals(node, "rtp.write_pacing.enabled",
                     boolOption(output.writePacingEnabled)) &&
        optionEquals(node, "rtp.write_pacing.bytes_per_second",
                     std::to_string(output.writePacingBytesPerSecond)) &&
        optionEquals(node, "rtp.write_pacing.burst_bytes",
                     std::to_string(output.writePacingBurstBytes)) &&
        optionalOptionEquals(node, "media_id", output.mediaId);
}

bool matchesMux(
    const MediaNode& node,
    const MediaRealtimeMuxNodePlan& mux)
{
    return optionEquals(node, MediaTranscodeOptionKey::MuxExpectVideo,
                        boolOption(mux.expectVideo)) &&
        optionEquals(node, MediaTranscodeOptionKey::MuxExpectAudio,
                     boolOption(mux.expectAudio)) &&
        optionEquals(node, "rtp.pacing.enabled",
                     boolOption(mux.pacingPolicy.enablePacing)) &&
        optionEquals(node, "rtp.packet_timestamps.monotonic",
                     boolOption(mux.monotonicPacketTimestamps)) &&
        optionEquals(node, "rtp.startup_delay_ms",
                     std::to_string(mux.startupDelayMs));
}

::media::Status validateSeparateAdapter(
    const MediaGraph& graph,
    const MediaRealtimeVideoRuntimePlan& runtime,
    const MediaRealtimeVideoSeparateRtpAdapterPlan& adapter)
{
    const MediaAvSyncGraphShape shape(graph);
    auto cardinality = shape.requireExact({
        {MediaNodeKind::RtpOutput, 1, "video RTP output"},
        {MediaNodeKind::RtpMux, 1, "video RTP mux"},
        {MediaNodeKind::SdpWriter, 1, "video SDP writer"},
        {MediaNodeKind::FileOutput, 0, "muxed output resource"},
        {MediaNodeKind::FileMux, 0, "muxed output"}},
        "VideoOnly separate RTP adapter");
    if (!cardinality) return cardinality;
    const MediaNode& output = *shape.nodes(MediaNodeKind::RtpOutput).front();
    const MediaNode& mux = *shape.nodes(MediaNodeKind::RtpMux).front();
    const MediaNode& sdp = *shape.nodes(MediaNodeKind::SdpWriter).front();
    if (!matchesRtpOutput(output, adapter.output) ||
        !matchesRtpOutput(mux, adapter.output) ||
        !matchesMux(mux, adapter.mux) ||
        !optionEquals(sdp, "path", adapter.sdp.path) ||
        !optionEquals(sdp, "sdp.expected_contexts",
                      std::to_string(adapter.sdp.expectedContexts)) ||
        !optionalOptionEquals(sdp, "media_id", adapter.sdp.mediaId) ||
        !hasExactSchedulerEdges(graph,
                                *shape.nodes(
                                    MediaNodeKind::VideoOutputScheduler).front(),
                                mux,
                                runtime)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly separate RTP DAG differs from its runtime adapter product"));
    }
    return ::media::Status::success();
}

::media::Status validateMuxedAdapter(
    const MediaGraph& graph,
    const MediaRealtimeVideoRuntimePlan& runtime,
    const MediaRealtimeVideoMuxedAdapterPlan& adapter)
{
    const MediaAvSyncGraphShape shape(graph);
    auto cardinality = shape.requireExact({
        {MediaNodeKind::RtpOutput, 0, "video RTP output"},
        {MediaNodeKind::RtpMux, 0, "video RTP mux"},
        {MediaNodeKind::SdpWriter, 0, "video SDP writer"},
        {MediaNodeKind::FileOutput, 1, "muxed output resource"},
        {MediaNodeKind::FileMux, 1, "muxed output"}},
        "VideoOnly muxed adapter");
    if (!cardinality) return cardinality;
    const MediaNode& output = *shape.nodes(MediaNodeKind::FileOutput).front();
    const MediaNode& mux = *shape.nodes(MediaNodeKind::FileMux).front();
    if (!adapter.output.outputResourceKind ||
        !adapter.output.muxSessionKind) {
        return ::media::Status::failure(::media::ErrorInfo::notInitialized(
            "VideoOnly muxed runtime adapter is incomplete"));
    }
    auto resource = mediaOutputResourceKindOptionValue(
        *adapter.output.outputResourceKind);
    auto session = mediaMuxSessionKindOptionValue(
        *adapter.output.muxSessionKind);
    if (!resource || !session) {
        return ::media::Status::failure(
            resource ? session.error() : resource.error());
    }
    if (!optionEquals(output, "url", adapter.output.url) ||
        !optionalOptionEquals(output, "format", adapter.output.format) ||
        !optionEquals(output, MediaTranscodeOptionKey::OutputResourceKind,
                      resource.value()) ||
        !optionEquals(mux, MediaTranscodeOptionKey::MuxExpectVideo,
                      boolOption(adapter.mux.expectVideo)) ||
        !optionEquals(mux, MediaTranscodeOptionKey::MuxExpectAudio,
                      boolOption(adapter.mux.expectAudio)) ||
        !optionEquals(mux, MediaTranscodeOptionKey::MuxSessionKind,
                      session.value()) ||
        !hasExactSchedulerEdges(graph,
                                *shape.nodes(
                                    MediaNodeKind::VideoOutputScheduler).front(),
                                mux,
                                runtime)) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            "VideoOnly muxed DAG differs from its runtime adapter product"));
    }
    return ::media::Status::success();
}

} // namespace

::media::Status MediaRealtimeVideoGraphShapeValidator::validate(
    const MediaGraph& graph,
    const MediaRealtimeVideoRuntimeBinding& binding)
{
    const auto& runtime = binding.runtime;
    const auto invalid = [](const char* field) {
        return ::media::Status::failure(::media::ErrorInfo::invalidArgument(
            std::string("Invalid VideoOnly runtime graph shape: ") + field));
    };
    const MediaNode* scheduler = nullptr;
    for (const MediaNode& node : graph.nodes()) {
        if (audioOrAvNode(node.kind)) return invalid("audio or A/V node");
        if (node.kind == MediaNodeKind::VideoOutputScheduler) {
            if (scheduler) return invalid("duplicate video scheduler");
            scheduler = &node;
        }
        for (const MediaPort& port : node.inputPorts) {
            if (port.streamKind == MediaStreamKind::Audio) {
                return invalid("audio input port");
            }
        }
        for (const MediaPort& port : node.outputPorts) {
            if (port.streamKind == MediaStreamKind::Audio) {
                return invalid("audio output port");
            }
        }
    }
    if (!scheduler || scheduler->inputPorts.size() != 1 ||
        scheduler->outputPorts.size() != 1 ||
        scheduler->inputPorts.front().name != "video" ||
        scheduler->outputPorts.front().name != "scheduled_video" ||
        scheduler->options.values().size() != 14) {
        return invalid("scheduler cardinality");
    }
    auto requireKeyFrame = requiredBoolNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.require_key_frame");
    auto maximumWait = requiredPositiveInt64NodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.maximum_wait_ns");
    auto packetCapacity = requiredPositiveInt64NodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.packet_capacity");
    auto maximumUnitBytes = requiredPositiveInt64NodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.maximum_unit_bytes");
    auto byteCapacity = requiredPositiveInt64NodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.startup.byte_capacity");
    auto sourceNumerator = requiredPositiveIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.source_time_base.num");
    auto sourceDenominator = requiredPositiveIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.source_time_base.den");
    auto frameRateNumerator = requiredPositiveIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.output_frame_rate.num");
    auto frameRateDenominator = requiredPositiveIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.output_frame_rate.den");
    auto packetTimeBaseNumerator = requiredPositiveIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_time_base.num");
    auto packetTimeBaseDenominator = requiredPositiveIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_time_base.den");
    auto packetTimingMode = requiredNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.packet_timing_mode");
    auto transportLead = requiredNonNegativeIntNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.transport_lead_ns");
    auto pacingEnabled = requiredBoolNodeOption(
        &scheduler->options, "MediaVideoOutputSchedulerNode",
        "video_scheduler.pacing_enabled");
    const char* expectedTimingMode = runtime.timing.packetTimingMode ==
            MediaRealtimeVideoPacketTimingMode::SourceTimeBase
        ? "source_time_base"
        : runtime.timing.packetTimingMode ==
                MediaRealtimeVideoPacketTimingMode::OutputCadenceTimeBase
            ? "output_cadence_time_base"
            : nullptr;
    if (!requireKeyFrame || !maximumWait || !packetCapacity ||
        !maximumUnitBytes || !byteCapacity || !sourceNumerator ||
        !sourceDenominator || !frameRateNumerator ||
        !frameRateDenominator || !packetTimeBaseNumerator ||
        !packetTimeBaseDenominator || !packetTimingMode ||
        !transportLead || !pacingEnabled || !expectedTimingMode ||
        requireKeyFrame.value() != runtime.startup.requireKeyFrame ||
        maximumWait.value() != runtime.startup.maximumWait.nanoseconds() ||
        static_cast<std::uint64_t>(packetCapacity.value()) !=
            runtime.startup.packetCapacity ||
        static_cast<std::uint64_t>(maximumUnitBytes.value()) !=
            runtime.startup.maximumUnitBytes ||
        static_cast<std::uint64_t>(byteCapacity.value()) !=
            runtime.startup.byteCapacity ||
        sourceNumerator.value() != runtime.timing.sourceTimeBase.num ||
        sourceDenominator.value() != runtime.timing.sourceTimeBase.den ||
        frameRateNumerator.value() != runtime.timing.outputFrameRate.num ||
        frameRateDenominator.value() != runtime.timing.outputFrameRate.den ||
        packetTimeBaseNumerator.value() !=
            runtime.timing.scheduledPacketTimeBase.num ||
        packetTimeBaseDenominator.value() !=
            runtime.timing.scheduledPacketTimeBase.den ||
        packetTimingMode.value() != expectedTimingMode ||
        transportLead.value() !=
            runtime.scheduling.transportLead.nanoseconds() ||
        pacingEnabled.value() != runtime.scheduling.pacingEnabled) {
        return invalid("scheduler options differ from runtime product");
    }
    if (const auto* separate =
            std::get_if<MediaRealtimeVideoSeparateRtpAdapterPlan>(
                &runtime.outputAdapter)) {
        return validateSeparateAdapter(graph, runtime, *separate);
    }
    if (const auto* muxed =
            std::get_if<MediaRealtimeVideoMuxedAdapterPlan>(
                &runtime.outputAdapter)) {
        return validateMuxedAdapter(graph, runtime, *muxed);
    }
    return invalid("unknown output adapter variant");
}

::media::Status MediaRealtimeVideoGraphShapeValidator::validateAbsent(
    const MediaGraph& graph)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::VideoOutputScheduler) {
            return ::media::Status::failure(
                ::media::ErrorInfo::invalidArgument(
                    "Non-VideoOnly graph rejects a video scheduler"));
        }
    }
    return ::media::Status::success();
}

} // namespace media::ffmpeg::graph
