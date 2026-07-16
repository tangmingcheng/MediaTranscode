#include "internal/graph/builder/segments/MediaRealtimeAvSyncInputSegmentBuilder.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/avsync/MediaAvSyncPlanValidator.h"

#include <string_view>
#include <tuple>
#include <variant>

namespace media::ffmpeg::graph {
namespace {

constexpr std::string_view Owner = "MediaRealtimeAvSyncInputSegmentBuilder";

struct ProtocolEndpoints final {
    MediaEndpoint video;
    MediaEndpoint audio;
    MediaEndpoint sourceClock;
};

::media::Result<void> setOption(MediaGraph& graph,
                                MediaNodeId node,
                                std::string_view key,
                                std::string value)
{
    return MediaGraphBuildSupport::setNodeOptionChecked(
        graph, Owner, node, key, value);
}

::media::Result<void> addInput(MediaGraph& graph,
                               MediaNodeId node,
                               std::string name,
                               MediaStreamKind stream,
                               MediaEdgeKind edge,
                               MediaPayloadKind payload)
{
    return MediaGraphBuildSupport::addInputPortChecked(
        graph, Owner, node, std::move(name), stream, edge, payload, true, false);
}

::media::Result<void> addOutput(MediaGraph& graph,
                                MediaNodeId node,
                                std::string name,
                                MediaStreamKind stream,
                                MediaEdgeKind edge,
                                MediaPayloadKind payload,
                                bool required = true,
                                bool multiple = false)
{
    return MediaGraphBuildSupport::addOutputPortChecked(
        graph, Owner, node, std::move(name), stream, edge, payload,
        required, multiple);
}

::media::Result<void> validateOutput(MediaGraph& graph,
                                     const MediaEndpoint& endpoint,
                                     MediaStreamKind stream,
                                     MediaEdgeKind edge,
                                     MediaPayloadKind payload)
{
    const MediaNode* node = graph.findNode(endpoint.node);
    if (!node || endpoint.port.empty()) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input segment requires valid source endpoints"));
    }
    const MediaPort* existing = graph.findOutputPort(endpoint.node, endpoint.port);
    if (!existing) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input source endpoint does not exist"));
    }
    if (existing->streamKind != stream || existing->edgeKind != edge ||
        existing->payloadKind != payload || !existing->multiple) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input source endpoint does not match its planned type"));
    }
    return ::media::Result<void>::success();
}

::media::Result<void> connect(MediaGraph& graph,
                              const MediaEndpoint& from,
                              MediaNodeId to,
                              const char* toPort,
                              std::string label,
                              const MediaEdgePolicy& policy)
{
    return MediaGraphBuildSupport::connectChecked(
        graph, Owner, from.node, from.port, to, toPort, std::move(label),
        policy);
}

::media::Result<void> connect(MediaGraph& graph,
                              MediaNodeId from,
                              const char* fromPort,
                              MediaNodeId to,
                              const char* toPort,
                              std::string label,
                              const MediaEdgePolicy& policy)
{
    return MediaGraphBuildSupport::connectChecked(
        graph, Owner, from, fromPort, to, toPort, std::move(label), policy);
}

::media::Result<MediaNodeId> addNode(MediaGraph& graph,
                                     MediaNodeKind kind,
                                     std::string name,
                                     std::string diagnostic)
{
    MediaNodeId node = graph.addNode(kind, std::move(name), std::move(diagnostic));
    if (!node.isValid()) {
        return ::media::Result<MediaNodeId>::failure(
            ::media::ErrorInfo::internalError(
                "Synchronized input segment failed to add a node"));
    }
    return ::media::Result<MediaNodeId>::success(node);
}

::media::Result<void> configureRtpBinder(
    MediaGraph& graph,
    MediaNodeId node,
    MediaStreamKind stream,
    const MediaCanonicalVideoAssemblyPlan* video,
    const MediaCanonicalAudioAssemblyPlan* audio,
    const MediaAvSyncGroupKey& group)
{
    const bool isVideo = stream == MediaStreamKind::Video;
    const std::size_t capacity = isVideo
        ? video->acquiringCapacity : audio->acquiringCapacity;
    const MediaRunningTime timeout = isVideo
        ? video->acquiringTimeout : audio->acquiringTimeout;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.stream",
            isVideo ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.acquiring_capacity",
            std::to_string(capacity)); !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.acquiring_timeout_ns",
            std::to_string(timeout.nanoseconds())); !status) return status;
    if (auto status = setOption(
            graph, node, "rtp_clock_binder.sync_group", group.value());
        !status) return status;
    if (isVideo) {
        const auto* duration = std::get_if<MediaRtpTimestampDeltaDurationPlan>(
            &video->duration);
        if (!duration || duration->clockRate <= 0 ||
            duration->terminalPolicy !=
                MediaTerminalDurationPolicy::RepeatLastObservedPositiveDelta) {
            return ::media::Result<void>::failure(
                ::media::ErrorInfo::invalidArgument(
                    "RTP video binder requires the complete planned duration policy"));
        }
        if (auto status = setOption(
                graph, node, "rtp_clock_binder.duration_clock_rate",
                std::to_string(duration->clockRate)); !status) return status;
        if (auto status = setOption(
                graph, node, "rtp_clock_binder.terminal_duration_policy",
                "repeat_last_observed_positive_delta"); !status) return status;
    }
    return ::media::Result<void>::success();
}

::media::Result<ProtocolEndpoints> buildRtpProtocol(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (!std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
            plan.assembly.inputClock)) {
        return ::media::Result<ProtocolEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "RTP synchronized input requires an RTP assembly plan"));
    }
    if (auto status = validateOutput(
            graph, options.sources.videoPacket, MediaStreamKind::Video,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) {
        return ::media::Result<ProtocolEndpoints>::failure(status.error());
    }
    if (auto status = validateOutput(
            graph, options.sources.audioPacket, MediaStreamKind::Audio,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) {
        return ::media::Result<ProtocolEndpoints>::failure(status.error());
    }
    if (auto status = validateOutput(
            graph, options.sources.protocolClock, MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) {
        return ::media::Result<ProtocolEndpoints>::failure(status.error());
    }

    auto snapshotResult = addNode(
        graph, MediaNodeKind::RtpClockSnapshotFanout,
        options.prefix + ".rtp.clock_snapshot", "RTP clock snapshot fanout");
    auto videoResult = addNode(
        graph, MediaNodeKind::RtpPacketClockBinder,
        options.prefix + ".video.protocol_binder", "RTP video clock binder");
    auto audioResult = addNode(
        graph, MediaNodeKind::RtpPacketClockBinder,
        options.prefix + ".audio.protocol_binder", "RTP audio clock binder");
    auto adapterResult = addNode(
        graph, MediaNodeKind::RtpSourceClockStateAdapter,
        options.prefix + ".rtp.source_clock_adapter",
        "RTP source clock state adapter");
    if (!snapshotResult || !videoResult || !audioResult || !adapterResult) {
        const auto& error = !snapshotResult ? snapshotResult.error()
            : !videoResult ? videoResult.error()
            : !audioResult ? audioResult.error() : adapterResult.error();
        return ::media::Result<ProtocolEndpoints>::failure(error);
    }
    const MediaNodeId snapshot = snapshotResult.value();
    const MediaNodeId video = videoResult.value();
    const MediaNodeId audio = audioResult.value();
    const MediaNodeId adapter = adapterResult.value();

    if (auto status = addInput(graph, snapshot, "clock", MediaStreamKind::Metadata,
                               MediaEdgeKind::Event,
                               MediaPayloadKind::GraphEvent); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    for (const char* port : {"video", "audio", "startup"}) {
        if (auto status = addOutput(graph, snapshot, port,
                                    MediaStreamKind::Metadata,
                                    MediaEdgeKind::Event,
                                    MediaPayloadKind::GraphEvent, true, false);
            !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    }
    for (const auto [node, stream] : {
             std::pair{video, MediaStreamKind::Video},
             std::pair{audio, MediaStreamKind::Audio}}) {
        if (auto status = addInput(graph, node, "packet", stream,
                                   MediaEdgeKind::InputPacket,
                                   MediaPayloadKind::Packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = addInput(graph, node, "clock", MediaStreamKind::Metadata,
                                   MediaEdgeKind::Event,
                                   MediaPayloadKind::GraphEvent); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = addOutput(graph, node, "packet", stream,
                                    MediaEdgeKind::InputPacket,
                                    MediaPayloadKind::Packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    }
    if (auto status = addInput(graph, adapter, "clock", MediaStreamKind::Metadata,
                               MediaEdgeKind::Event,
                               MediaPayloadKind::GraphEvent); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, adapter, "state", MediaStreamKind::Metadata,
                                MediaEdgeKind::Event,
                                MediaPayloadKind::GraphEvent); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());

    if (auto status = configureRtpBinder(
            graph, video, MediaStreamKind::Video, &plan.assembly.video,
            &plan.assembly.audio, plan.groupKey); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = configureRtpBinder(
            graph, audio, MediaStreamKind::Audio, &plan.assembly.video,
            &plan.assembly.audio, plan.groupKey); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());

    const auto& metadata = plan.edgePolicies.metadata;
    const auto& packet = plan.edgePolicies.synchronizedPacket;
    if (auto status = connect(graph, options.sources.protocolClock, snapshot,
                              "clock", "RTP group clock -> snapshot fanout",
                              metadata); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = connect(graph, snapshot, "video", video, "clock",
                              "RTP snapshot -> video binder", metadata); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = connect(graph, snapshot, "audio", audio, "clock",
                              "RTP snapshot -> audio binder", metadata); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = connect(graph, snapshot, "startup", adapter, "clock",
                              "RTP snapshot -> source clock adapter", metadata); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = connect(graph, options.sources.videoPacket, video,
                              "packet", "RTP video packet -> clock binder",
                              packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = connect(graph, options.sources.audioPacket, audio,
                              "packet", "RTP audio packet -> clock binder",
                              packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());

    return ::media::Result<ProtocolEndpoints>::success(ProtocolEndpoints{
        MediaEndpoint{video, "packet"}, MediaEndpoint{audio, "packet"},
        MediaEndpoint{adapter, "state"}});
}

::media::Result<ProtocolEndpoints> buildMpegTsProtocol(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (!std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(
            plan.assembly.inputClock)) {
        return ::media::Result<ProtocolEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "MPEG-TS synchronized input requires a TS assembly plan"));
    }
    if (auto status = validateOutput(
            graph, options.sources.videoPacket, MediaStreamKind::Video,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = validateOutput(
            graph, options.sources.audioPacket, MediaStreamKind::Audio,
            MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = validateOutput(
            graph, options.sources.protocolClock, MediaStreamKind::Metadata,
            MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());

    auto videoResult = addNode(
        graph, MediaNodeKind::InitialLockedPacketGate,
        options.prefix + ".video.protocol_binder", "MPEG-TS video lock gate");
    auto audioResult = addNode(
        graph, MediaNodeKind::InitialLockedPacketGate,
        options.prefix + ".audio.protocol_binder", "MPEG-TS audio lock gate");
    if (!videoResult || !audioResult) {
        return ::media::Result<ProtocolEndpoints>::failure(
            !videoResult ? videoResult.error() : audioResult.error());
    }
    const MediaNodeId video = videoResult.value();
    const MediaNodeId audio = audioResult.value();
    for (const auto [node, stream, assembly] : {
             std::tuple{video, MediaStreamKind::Video,
                        plan.assembly.video.acquiringTimeout},
             std::tuple{audio, MediaStreamKind::Audio,
                        plan.assembly.audio.acquiringTimeout}}) {
        if (auto status = addInput(graph, node, "packet", stream,
                                   MediaEdgeKind::InputPacket,
                                   MediaPayloadKind::Packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = addInput(graph, node, "clock", MediaStreamKind::Metadata,
                                   MediaEdgeKind::Event,
                                   MediaPayloadKind::GraphEvent); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = addOutput(graph, node, "packet", stream,
                                    MediaEdgeKind::InputPacket,
                                    MediaPayloadKind::Packet); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = setOption(graph, node, "initial_locked_gate.stream",
                                    stream == MediaStreamKind::Video
                                        ? "video" : "audio"); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = setOption(
                graph, node, "initial_locked_gate.acquiring_timeout_ns",
                std::to_string(assembly.nanoseconds())); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
        if (auto status = setOption(graph, node, "initial_locked_gate.sync_group",
                                    plan.groupKey.value()); !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    }
    const auto& packet = plan.edgePolicies.synchronizedPacket;
    if (auto status = connect(graph, options.sources.videoPacket, video,
                              "packet", "MPEG-TS video -> lock gate", packet);
        !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    if (auto status = connect(graph, options.sources.audioPacket, audio,
                              "packet", "MPEG-TS audio -> lock gate", packet);
        !status) return ::media::Result<ProtocolEndpoints>::failure(status.error());
    return ::media::Result<ProtocolEndpoints>::success(ProtocolEndpoints{
        MediaEndpoint{video, "packet"}, MediaEndpoint{audio, "packet"},
        options.sources.protocolClock});
}

::media::Result<void> configureCanonical(
    MediaGraph& graph,
    MediaNodeId node,
    MediaScheduledStream stream,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const auto& assembly = plan.assembly;
    const bool video = stream == MediaScheduledStream::Video;
    if (auto status = setOption(graph, node, "canonical_input.stream",
                                video ? "video" : "audio"); !status) return status;
    if (auto status = setOption(
            graph, node, "canonical_input.source_identity",
            video ? assembly.video.sourceIdentity
                  : assembly.audio.sourceIdentity); !status) return status;
    const MediaDecodeOrderMode order = video
        ? assembly.video.decodeOrder : assembly.audio.decodeOrder;
    if (auto status = setOption(
            graph, node, "canonical_input.decode_order",
            order == MediaDecodeOrderMode::ReorderedRequiresDecodeTime
                ? "reordered" : "presentation"); !status) return status;
    if (video) {
        if (auto status = setOption(graph, node,
                                    "canonical_input.duration_source",
                                    "packet"); !status) return status;
        return ::media::Result<void>::success();
    }
    if (const auto* samples =
            std::get_if<MediaPlannedAudioSamplesDurationPlan>(
                &assembly.audio.duration)) {
        if (auto status = setOption(graph, node,
                                    "canonical_input.duration_source",
                                    "audio_samples"); !status) return status;
        if (auto status = setOption(
                graph, node, "canonical_input.audio_sample_count",
                std::to_string(samples->samplesPerAccessUnit)); !status) return status;
        if (auto status = setOption(
                graph, node, "canonical_input.audio_sample_rate",
                std::to_string(samples->sampleRate)); !status) return status;
        return ::media::Result<void>::success();
    }
    if (!std::holds_alternative<MediaPacketDurationPlan>(
            assembly.audio.duration)) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical audio input requires a planned duration source"));
    }
    if (auto status = setOption(graph, node,
                                "canonical_input.duration_source", "packet");
        !status) return status;
    if (!plan.planningFacts.inputAudioSampleRate ||
        *plan.planningFacts.inputAudioSampleRate <= 0) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Canonical MPEG-TS audio requires the planned input sample rate"));
    }
    return setOption(graph, node, "canonical_input.audio_sample_rate",
                     std::to_string(*plan.planningFacts.inputAudioSampleRate));
}

::media::Result<void> configureCoordinator(
    MediaGraph& graph,
    MediaNodeId node,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    const auto& startup = plan.synchronization.startup;
    const auto required = startup.requireVideoKeyFrame &&
        startup.trimAudioToCommonStart && startup.maximumWaitNs &&
        startup.prerollNs && startup.keyFrameWaitNs &&
        startup.maximumAudioTrimNs && startup.maximumInitialSkewNs &&
        startup.maximumGapNs && startup.outputLeadNs &&
        startup.videoCapacity && startup.audioCapacity &&
        startup.videoByteCapacity && startup.audioByteCapacity &&
        startup.maximumVideoUnitBytes && startup.maximumAudioUnitBytes &&
        startup.videoIdentity && startup.audioIdentity &&
        startup.allowDegradedClock && plan.synchronization.topology;
    if (!required) {
        return ::media::Result<void>::failure(
            ::media::ErrorInfo::invalidArgument(
                "A/V startup coordinator requires a complete planner product"));
    }
    const auto setBool = [&](const char* key, bool value) {
        return setOption(graph, node, key, value ? "1" : "0");
    };
    const auto setTime = [&](const char* key, MediaRunningTime value) {
        return setOption(graph, node, key,
                         std::to_string(value.nanoseconds()));
    };
    if (auto status = setBool("av_startup.require_video_key_frame",
                              *startup.requireVideoKeyFrame); !status) return status;
    if (auto status = setBool("av_startup.trim_audio_to_common_start",
                              *startup.trimAudioToCommonStart); !status) return status;
    if (auto status = setBool("av_startup.allow_degraded_clock",
                              *startup.allowDegradedClock); !status) return status;
    if (auto status = setTime("av_startup.maximum_wait_ns", *startup.maximumWaitNs); !status) return status;
    if (auto status = setTime("av_startup.preroll_ns", *startup.prerollNs); !status) return status;
    if (auto status = setTime("av_startup.key_frame_wait_ns", *startup.keyFrameWaitNs); !status) return status;
    if (auto status = setTime("av_startup.maximum_audio_trim_ns", *startup.maximumAudioTrimNs); !status) return status;
    if (auto status = setTime("av_startup.maximum_initial_skew_ns", *startup.maximumInitialSkewNs); !status) return status;
    if (auto status = setTime("av_startup.maximum_gap_ns", *startup.maximumGapNs); !status) return status;
    if (auto status = setTime("av_startup.output_lead_ns", *startup.outputLeadNs); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.video_capacity", std::to_string(*startup.videoCapacity)); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.audio_capacity", std::to_string(*startup.audioCapacity)); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.video_byte_capacity", std::to_string(*startup.videoByteCapacity)); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.audio_byte_capacity", std::to_string(*startup.audioByteCapacity)); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.maximum_video_unit_bytes", std::to_string(*startup.maximumVideoUnitBytes)); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.maximum_audio_unit_bytes", std::to_string(*startup.maximumAudioUnitBytes)); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.video_identity", *startup.videoIdentity); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.audio_identity", *startup.audioIdentity); !status) return status;
    if (auto status = setOption(graph, node, "av_startup.sync_group", plan.groupKey.value()); !status) return status;
    return setOption(
        graph, node, "av_startup.topology",
        *plan.synchronization.topology ==
                MediaAvSyncTopology::SeparateRtpToSeparateRtp
            ? "separate_rtp" : "mpegts");
}

} // namespace

::media::Result<MediaRealtimeAvSyncInputEndpoints>
MediaRealtimeAvSyncInputSegmentBuilder::build(
    MediaGraph& graph,
    const MediaRealtimeAvSyncInputSegmentOptions& options,
    const MediaRealtimeAvSyncRuntimePlan& plan)
{
    if (options.prefix.empty() || !options.sources.videoPacket.valid() ||
        !options.sources.audioPacket.valid() ||
        !options.sources.protocolClock.valid() || !plan.groupKey.valid() ||
        !MediaAvSyncPlanValidator::validate(plan.synchronization)) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::invalidArgument(
                "Synchronized input segment requires complete planned inputs"));
    }
    auto protocol = std::holds_alternative<MediaRtpInputClockAssemblyPlan>(
        plan.assembly.inputClock)
        ? buildRtpProtocol(graph, options, plan)
        : buildMpegTsProtocol(graph, options, plan);
    if (!protocol) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            protocol.error());
    }

    auto sourceClockResult = addNode(
        graph, MediaNodeKind::SourceClockStateFanout,
        options.prefix + ".source_clock", "Source clock state fanout");
    auto videoCanonicalResult = addNode(
        graph, MediaNodeKind::CanonicalInput,
        options.prefix + ".video.canonical_input", "Canonical video input");
    auto audioCanonicalResult = addNode(
        graph, MediaNodeKind::CanonicalInput,
        options.prefix + ".audio.canonical_input", "Canonical audio input");
    auto coordinatorResult = addNode(
        graph, MediaNodeKind::AvStartupCoordinator,
        options.prefix + ".startup.coordinator", "A/V startup coordinator");
    auto startupClockResult = addNode(
        graph, MediaNodeKind::AvStartupClock,
        options.prefix + ".startup.clock", "A/V startup master-clock tick");
    auto binderResult = addNode(
        graph, MediaNodeKind::PlaybackEpochBinder,
        options.prefix + ".startup.epoch_binder", "Playback epoch binder");
    auto sequencerResult = addNode(
        graph, MediaNodeKind::ActivatedStartupReleaseSequencer,
        options.prefix + ".startup.activation_sequencer",
        "Activated startup release sequencer");
    auto extractorResult = addNode(
        graph, MediaNodeKind::AvBoundReleaseExtractor,
        options.prefix + ".startup.release_extractor",
        "Atomic A/V release extractor");
    if (!sourceClockResult || !videoCanonicalResult || !audioCanonicalResult ||
        !coordinatorResult || !startupClockResult || !binderResult ||
        !sequencerResult || !extractorResult) {
        return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(
            ::media::ErrorInfo::internalError(
                "Synchronized input segment failed to assemble shared nodes"));
    }
    const MediaNodeId sourceClock = sourceClockResult.value();
    const MediaNodeId videoCanonical = videoCanonicalResult.value();
    const MediaNodeId audioCanonical = audioCanonicalResult.value();
    const MediaNodeId coordinator = coordinatorResult.value();
    const MediaNodeId startupClock = startupClockResult.value();
    const MediaNodeId binder = binderResult.value();
    const MediaNodeId sequencer = sequencerResult.value();
    const MediaNodeId extractor = extractorResult.value();

    if (auto status = addInput(graph, sourceClock, "clock", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    for (const char* port : {"video", "audio", "startup"}) {
        const bool tsGateClock = std::string_view(port) != "startup";
        if (auto status = addOutput(graph, sourceClock, port, MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, !tsGateClock || std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(plan.assembly.inputClock), false); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    }
    for (const auto [node, stream] : {
             std::pair{videoCanonical, MediaStreamKind::Video},
             std::pair{audioCanonical, MediaStreamKind::Audio}}) {
        if (auto status = addInput(graph, node, "in", stream, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
        if (auto status = addOutput(graph, node, "out", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    }
    for (const char* port : {"video", "audio", "clock"}) {
        if (auto status = addInput(graph, coordinator, port, MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    }
    if (auto status = addOutput(graph, coordinator, "release", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addInput(graph, startupClock, "clock", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, startupClock, "tick", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addInput(graph, binder, "release", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, binder, "transaction", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addInput(graph, sequencer, "transaction", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, sequencer, "activated", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent, false, true); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, sequencer, "bound_release", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addInput(graph, extractor, "in", MediaStreamKind::Metadata, MediaEdgeKind::Event, MediaPayloadKind::GraphEvent); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, extractor, "video", MediaStreamKind::Video, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = addOutput(graph, extractor, "audio", MediaStreamKind::Audio, MediaEdgeKind::InputPacket, MediaPayloadKind::Packet, false, true); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());

    if (auto status = configureCanonical(graph, videoCanonical, MediaScheduledStream::Video, plan); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = configureCanonical(graph, audioCanonical, MediaScheduledStream::Audio, plan); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = configureCoordinator(graph, coordinator, plan); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = setOption(graph, startupClock, "av_startup_clock.sync_group", plan.groupKey.value()); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = setOption(graph, startupClock, "av_startup_clock.interval_ns", std::to_string(plan.assembly.startupClockInterval.nanoseconds())); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = setOption(graph, binder, "playback_epoch_binder.sync_group", plan.groupKey.value()); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = setOption(graph, sequencer, "activated_startup_release_sequencer.sync_group", plan.groupKey.value()); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());

    const auto& metadata = plan.edgePolicies.metadata;
    const auto& packet = plan.edgePolicies.synchronizedPacket;
    if (auto status = connect(graph, protocol.value().sourceClock, sourceClock, "clock", "protocol clock -> source clock fanout", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (std::holds_alternative<MediaMpegTsInputClockAssemblyPlan>(plan.assembly.inputClock)) {
        if (auto status = connect(graph, sourceClock, "video", protocol.value().video.node, "clock", "source clock -> video lock gate", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
        if (auto status = connect(graph, sourceClock, "audio", protocol.value().audio.node, "clock", "source clock -> audio lock gate", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    }
    if (auto status = connect(graph, protocol.value().video, videoCanonical, "in", "clock-bound video -> canonical input", packet); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, protocol.value().audio, audioCanonical, "in", "clock-bound audio -> canonical input", packet); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, videoCanonical, "out", coordinator, "video", "canonical video -> startup coordinator", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, audioCanonical, "out", coordinator, "audio", "canonical audio -> startup coordinator", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, sourceClock, "startup", startupClock, "clock", "source clock -> startup clock", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, startupClock, "tick", coordinator, "clock", "master clock tick -> startup coordinator", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, coordinator, "release", binder, "release", "startup release -> epoch binder", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, binder, "transaction", sequencer, "transaction", "epoch transaction -> activation sequencer", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());
    if (auto status = connect(graph, sequencer, "bound_release", extractor, "in", "activated release -> atomic extractor", metadata); !status) return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::failure(status.error());

    return ::media::Result<MediaRealtimeAvSyncInputEndpoints>::success(
        MediaRealtimeAvSyncInputEndpoints{
            MediaEndpoint{extractor, "video"},
            MediaEndpoint{extractor, "audio"},
            MediaEndpoint{sequencer, "activated"}});
}

} // namespace media::ffmpeg::graph
