#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/planner/MediaBlockingEdgePolicyPlanner.h"
#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"
#include "internal/graph/builder/segments/MediaOutputSegmentBuilder.h"
#include "internal/graph/model/MediaMuxSessionKind.h"
#include "internal/graph/model/MediaOutputResourceKind.h"
#include "internal/graph/planner/local/MediaLocalFileOutputPlanner.h"

#include <optional>
#include <algorithm>
#include <array>
#include <type_traits>

using namespace media::ffmpeg::graph;
using media_transcode::test::TestContext;

namespace {

MediaAudioEncodeBranchOptions audioEncodeOptions(MediaGraph& graph)
{
    MediaAudioEncodeBranchOptions options;
    options.plan.enabled = true;
    options.plan.branchMode = MediaBranchMode::TranscodeFrame;
    options.plan.sourceStreamIndex = 1;
    MediaResolvedAudioSource source{
        "aac", MediaAudioProfile::knownAacLow(), 44'100, 2, "stereo", "fltp", 128'000};
    MediaResolvedAudioRequest request;
    request.sampleRate = 48'000;
    auto target = MediaResolvedAudioTargetDecision::create(source, request, {});
    auto resolved = MediaResolvedAudioOutputPlan::create(
        target.value(),
        std::optional<MediaSelectedAudioEncoder>{{
            "aac", "fltp", {},
            {MediaAudioProfile::knownAacLow().ffmpegProfileId()}, 1024, 0}},
        std::nullopt);
    options.plan.resolvedOutput = std::move(resolved).value();
    options.queues = MediaGraphQueueParameters{2, 4, 3, 4};
    options.normalizePackets = false;
    options.lineageMode = MediaAudioLineageExecutionMode::LegacyPlainPacket;
    options.formatSourceNode = graph.addNode(MediaNodeKind::DebugDump, "format_source");
    graph.addOutputPort(options.formatSourceNode, "format", MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    options.packetSourceNode = graph.addNode(MediaNodeKind::DebugDump, "packet_source");
    const MediaPortId packetSourcePort = graph.addOutputPort(
        options.packetSourceNode, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    graph.setPortFormatDescriptor(
        packetSourcePort,
        MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Audio, 1));
    options.edgePolicies = MediaBlockingEdgePolicyPlanner::plan(options.queues);
    return options;
}

const MediaNode* resampleNode(const MediaGraph& graph)
{
    for (const auto& node : graph.nodes()) {
        if (node.kind == MediaNodeKind::AudioResample) {
            return &node;
        }
    }
    return nullptr;
}

void testAudioCorrectionBuilderContract(TestContext& ctx)
{
    MediaGraph missingLineageModeGraph;
    auto missingLineageMode = audioEncodeOptions(missingLineageModeGraph);
    missingLineageMode.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
    missingLineageMode.lineageMode.reset();
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(
                          missingLineageModeGraph, missingLineageMode));

    MediaGraph incompleteGraph;
    auto incomplete = audioEncodeOptions(incompleteGraph);
    incomplete.plan.resolvedOutput.reset();
    incomplete.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(incompleteGraph, incomplete));
    EXPECT_TRUE(ctx, resampleNode(incompleteGraph) == nullptr);

    MediaGraph missingSourceGraph;
    auto missingSource = audioEncodeOptions(missingSourceGraph);
    missingSource.correctionMode = MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    missingSource.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    missingSource.lineageCapacity = 8;
    missingSource.correctionGeneration = 4;
    missingSource.correctionLookaheadWindows = 2;
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(missingSourceGraph, missingSource));
    EXPECT_TRUE(ctx, resampleNode(missingSourceGraph) == nullptr);

    MediaGraph unknownModeGraph;
    auto unknownMode = audioEncodeOptions(unknownModeGraph);
    unknownMode.correctionMode =
        static_cast<MediaAudioCorrectionExecutionMode>(255);
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(unknownModeGraph, unknownMode));
    EXPECT_TRUE(ctx, resampleNode(unknownModeGraph) == nullptr);

    MediaGraph externalGraph;
    auto external = audioEncodeOptions(externalGraph);
    external.correctionMode =
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    external.correctionGeneration = 4;
    external.correctionLookaheadWindows = 2;
    external.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    external.lineageCapacity = 8;
    external.syncGroup = MediaAvSyncGroupKey("builder.audio");
    EXPECT_TRUE(ctx, MediaAudioEncodeBranchBuilder::build(externalGraph, external));
    const MediaNode* externalResample = resampleNode(externalGraph);
    EXPECT_TRUE(ctx, externalResample != nullptr);
    if (externalResample) {
        EXPECT_TRUE(ctx, externalGraph.findInputPort(
                             externalResample->id, "correction") != nullptr);
        EXPECT_EQ(ctx, externalResample->options.value(
                           MediaAudioCorrectionOptionKey::Mode),
                  std::string("external_required"));
        EXPECT_EQ(ctx, externalResample->options.value(
                           MediaAudioCorrectionOptionKey::Generation),
                  std::string("4"));
        EXPECT_EQ(ctx, externalResample->options.value(
                           MediaAudioCorrectionOptionKey::LookaheadWindows),
                  std::string("2"));
    }

    MediaGraph disabledGraph;
    auto disabled = audioEncodeOptions(disabledGraph);
    disabled.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
    EXPECT_TRUE(ctx, MediaAudioEncodeBranchBuilder::build(disabledGraph, disabled));
    const MediaNode* resample = resampleNode(disabledGraph);
    EXPECT_TRUE(ctx, resample != nullptr);
    if (resample) {
        EXPECT_TRUE(ctx, disabledGraph.findInputPort(resample->id, "correction") == nullptr);
        EXPECT_EQ(ctx, resample->options.value("audio_correction.mode"), std::string("disabled"));
    }

    MediaGraph synchronizedGraph;
    auto synchronized = audioEncodeOptions(synchronizedGraph);
    synchronized.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
    synchronized.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    MediaGraph missingLineageCapacityGraph;
    auto missingLineageCapacity = audioEncodeOptions(missingLineageCapacityGraph);
    missingLineageCapacity.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
    missingLineageCapacity.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(
                          missingLineageCapacityGraph, missingLineageCapacity));

    synchronized.lineageCapacity = 8;
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(
                          synchronizedGraph, synchronized));

    MediaGraph invalidTransactionGraph;
    auto invalidTransaction = audioEncodeOptions(invalidTransactionGraph);
    invalidTransaction.correctionMode =
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    invalidTransaction.correctionGeneration = 1;
    invalidTransaction.correctionLookaheadWindows = 2;
    invalidTransaction.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    invalidTransaction.lineageCapacity = 8;
    invalidTransaction.syncGroup = MediaAvSyncGroupKey("builder.audio");
    invalidTransaction.edgePolicies.audioDriftTransaction.queuePolicy
        .orderingPolicy = MediaQueueOrderingPolicy::Timestamp;
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(
                          invalidTransactionGraph, invalidTransaction));
    EXPECT_TRUE(ctx, resampleNode(invalidTransactionGraph) == nullptr);

    MediaGraph productionGraph;
    auto production = audioEncodeOptions(productionGraph);
    production.correctionMode =
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    production.correctionGeneration = 1;
    production.correctionLookaheadWindows = 2;
    production.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    production.lineageCapacity = 8;
    production.syncGroup = MediaAvSyncGroupKey("builder.audio");
    auto productionResult =
        MediaAudioEncodeBranchBuilder::build(productionGraph, production);
    EXPECT_TRUE(ctx, productionResult);
    const auto trim = std::find_if(
        productionGraph.nodes().begin(), productionGraph.nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::AudioStartupTrim;
        });
    EXPECT_TRUE(ctx, trim != productionGraph.nodes().end());
    const std::array expectedLineageStages{
        std::pair{MediaNodeKind::AudioDecode, "audio_decoder_lineage_registry"},
        std::pair{MediaNodeKind::AudioStartupTrim, "audio_startup_trim_lineage_registry"},
        std::pair{MediaNodeKind::AudioResample, "audio_resampler_lineage_registry"},
        std::pair{MediaNodeKind::AudioEncode, "audio_encoder_lineage_registry"}};
    for (const auto& [kind, identity] : expectedLineageStages) {
        const auto stage = std::find_if(
            productionGraph.nodes().begin(), productionGraph.nodes().end(),
            [kind](const MediaNode& node) { return node.kind == kind; });
        EXPECT_TRUE(ctx, stage != productionGraph.nodes().end());
        if (stage != productionGraph.nodes().end()) {
            EXPECT_EQ(ctx, stage->options.value("audio.lineage.identity"),
                      std::string(identity));
            EXPECT_EQ(ctx, stage->options.value("audio.lineage.capacity"),
                      std::string("8"));
        }
    }
    const auto drift = std::find_if(
        productionGraph.nodes().begin(), productionGraph.nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::AudioDriftController;
        });
    const auto canonicalizer = std::find_if(
        productionGraph.nodes().begin(), productionGraph.nodes().end(),
        [](const MediaNode& node) {
            return node.kind == MediaNodeKind::EncodedAudioCanonicalizer;
        });
    EXPECT_TRUE(ctx, drift != productionGraph.nodes().end());
    EXPECT_TRUE(ctx, canonicalizer != productionGraph.nodes().end());
    if (drift != productionGraph.nodes().end()) {
        EXPECT_EQ(ctx, drift->options.value("audio_drift_controller.sync_group"),
                  std::string("builder.audio"));
        const auto transactionalPolicy =
            production.edgePolicies.audioDriftTransaction;
        const auto transactionEdges = std::count_if(
            productionGraph.edges().begin(), productionGraph.edges().end(),
            [&](const MediaEdge& edge) {
                if (edge.from.nodeId != drift->id) return false;
                EXPECT_EQ(ctx, edge.policy, transactionalPolicy);
                EXPECT_EQ(ctx, edge.policy.queuePolicy.overflowPolicy,
                          MediaQueueOverflowPolicy::BlockProducer);
                EXPECT_EQ(ctx, edge.policy.queuePolicy.orderingPolicy,
                          MediaQueueOrderingPolicy::Fifo);
                EXPECT_TRUE(ctx, edge.policy.queuePolicy.preserveOrdering);
                return true;
            });
        EXPECT_EQ(ctx, transactionEdges, std::size_t{2});
    }
    if (drift != productionGraph.nodes().end() &&
        canonicalizer != productionGraph.nodes().end()) {
        const auto hasEdge = [&](MediaNodeKind from, MediaNodeKind to) {
            const auto fromNode = std::find_if(
                productionGraph.nodes().begin(), productionGraph.nodes().end(),
                [from](const MediaNode& node) { return node.kind == from; });
            const auto toNode = std::find_if(
                productionGraph.nodes().begin(), productionGraph.nodes().end(),
                [to](const MediaNode& node) { return node.kind == to; });
            return fromNode != productionGraph.nodes().end() &&
                toNode != productionGraph.nodes().end() &&
                std::any_of(
                    productionGraph.edges().begin(), productionGraph.edges().end(),
                    [&](const MediaEdge& edge) {
                        return edge.from.nodeId == fromNode->id &&
                            edge.to.nodeId == toNode->id;
                    });
        };
        EXPECT_TRUE(ctx, hasEdge(
                             MediaNodeKind::AudioStartupTrim,
                             MediaNodeKind::AudioDriftController));
        EXPECT_TRUE(ctx, hasEdge(
                             MediaNodeKind::AudioDriftController,
                             MediaNodeKind::AudioResample));
        EXPECT_TRUE(ctx, hasEdge(
                             MediaNodeKind::AudioEncode,
                             MediaNodeKind::EncodedAudioCanonicalizer));
        EXPECT_FALSE(ctx, hasEdge(
                              MediaNodeKind::AvOutputScheduler,
                              MediaNodeKind::AudioResample));
    }
    if (productionResult) {
        const MediaNode* canonicalPacket =
            productionGraph.findNode(productionResult.value().packet.node);
        EXPECT_TRUE(ctx, canonicalPacket != nullptr);
        if (canonicalPacket) {
            EXPECT_EQ(ctx, canonicalPacket->kind,
                      MediaNodeKind::EncodedAudioCanonicalizer);
        }
        EXPECT_EQ(ctx, productionResult.value().packet.port,
                  std::string("canonical"));
    }
}

void testSynchronizedAudioPacketCopyIsUnsupported(TestContext& ctx)
{
    MediaGraph graph;
    MediaAudioBranchSegmentOptions options;
    options.plan.enabled = true;
    options.plan.branchMode = MediaBranchMode::CopyPacket;
    options.plan.sourceStreamIndex = 1;
    options.normalizeInputPackets = false;
    options.correctionMode =
        MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
    options.lineageMode =
        MediaAudioLineageExecutionMode::SynchronizedReleasedAudio;
    options.lineageCapacity = 8;
    options.correctionGeneration = 1;
    options.correctionLookaheadWindows = 2;
    options.syncGroup = MediaAvSyncGroupKey("builder.audio.copy");
    auto result = MediaAudioBranchSegmentBuilder::build(graph, options);
    EXPECT_FALSE(ctx, result);
    if (!result) {
        EXPECT_EQ(ctx, result.error().code, ::media::ErrorCode::Unsupported);
    }
}

void testTypedPacketCopyEndpointsPreserveOwnerBoundaries(TestContext& ctx)
{
    MediaGraph genericGraph;
    const MediaNodeId formatSource = genericGraph.addNode(
        MediaNodeKind::DebugDump, "generic.format");
    genericGraph.addOutputPort(
        formatSource, "format", MediaStreamKind::Metadata,
        MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    const MediaNodeId packetSource = genericGraph.addNode(
        MediaNodeKind::DebugDump, "generic.packet");
    const MediaPortId genericPacketPort = genericGraph.addOutputPort(
        packetSource, "audio", MediaStreamKind::Audio,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    genericGraph.setPortFormatDescriptor(
        genericPacketPort,
        MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Audio, 1));
    const MediaNodeId unrelatedMux = genericGraph.addNode(
        MediaNodeKind::DebugDump, "generic.unrelated_mux");

    MediaPacketCopyBranchOptions generic;
    generic.prefix = "generic.audio.copy";
    generic.streamKind = MediaStreamKind::Audio;
    generic.sourceStreamIndex = 1;
    generic.formatSourceNode = formatSource;
    generic.formatSourcePort = "format";
    generic.packetSourceNode = packetSource;
    generic.packetSourcePort = "audio";
    generic.normalizePackets = false;
    generic.queues.metadata = 1;
    generic.queues.packet = 1;
    generic.edgePolicies =
        MediaBlockingEdgePolicyPlanner::plan(generic.queues);
    auto genericResult = MediaPacketCopyBranchBuilder::build(
        genericGraph, generic);
    EXPECT_TRUE(ctx, genericResult);
    EXPECT_FALSE(ctx, std::any_of(
                          genericGraph.edges().begin(), genericGraph.edges().end(),
                          [unrelatedMux](const MediaEdge& edge) {
                              return edge.to.nodeId == unrelatedMux;
                          }));

    MediaGraph videoGraph;
    const MediaNodeId videoFormat = videoGraph.addNode(
        MediaNodeKind::DebugDump, "video.format");
    videoGraph.addOutputPort(
        videoFormat, "format", MediaStreamKind::Metadata,
        MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    const MediaNodeId videoPacket = videoGraph.addNode(
        MediaNodeKind::DebugDump, "video.packet");
    const MediaPortId videoPacketPort = videoGraph.addOutputPort(
        videoPacket, "video", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    videoGraph.setPortFormatDescriptor(
        videoPacketPort,
        MediaGraphBuildSupport::streamIndexDescriptor(MediaStreamKind::Video, 0));
    const MediaNodeId videoMux = videoGraph.addNode(
        MediaNodeKind::DebugDump, "video.mux");
    videoGraph.addInputPort(
        videoMux, "codec", MediaStreamKind::Video, MediaEdgeKind::Metadata,
        MediaPayloadKind::CodecParameters);
    videoGraph.addInputPort(
        videoMux, "packet", MediaStreamKind::Video,
        MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);

    MediaVideoPacketCopyBranchOptions video;
    video.prefix = "video.copy";
    video.plan.enabled = true;
    video.plan.branchMode = MediaBranchMode::CopyPacket;
    video.plan.sourceStreamIndex = 0;
    video.formatSourceNode = videoFormat;
    video.formatSourcePort = "format";
    video.packetSourceNode = videoPacket;
    video.packetSourcePort = "video";
    video.normalizePackets = false;
    video.queues.metadata = 1;
    video.queues.packet = 1;
    video.queues.mux = 1;
    video.edgePolicies =
        MediaBlockingEdgePolicyPlanner::plan(video.queues);
    auto videoResult = MediaVideoPacketCopyBranchBuilder::build(videoGraph, video);
    EXPECT_TRUE(ctx, videoResult);
    if (videoResult) {
        EXPECT_TRUE(ctx, videoResult.value().codec.valid());
        EXPECT_TRUE(ctx, videoResult.value().packet.valid());
    }
    const auto muxEdges = std::count_if(
        videoGraph.edges().begin(), videoGraph.edges().end(),
        [videoMux](const MediaEdge& edge) {
            return edge.to.nodeId == videoMux;
        });
    EXPECT_EQ(ctx, muxEdges, std::size_t{0});
}

void testFileMuxSessionKindIsExplicit(TestContext& ctx)
{
    FileOutputSegmentOptions missing;
    missing.outputUrl = "output.mp4";
    missing.queues.metadata = 1;
    MediaGraph missingGraph;
    EXPECT_FALSE(ctx, MediaOutputSegmentBuilder::buildFileMuxOutput(missingGraph, missing));
    EXPECT_TRUE(ctx, missingGraph.nodes().empty());

    FileOutputSegmentOptions missingResource = missing;
    missingResource.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    missingResource.expectVideo = true;
    MediaGraph missingResourceGraph;
    EXPECT_FALSE(ctx, MediaOutputSegmentBuilder::buildFileMuxOutput(
        missingResourceGraph, missingResource));
    EXPECT_TRUE(ctx, missingResourceGraph.nodes().empty());

    FileOutputSegmentOptions invalid = missing;
    invalid.muxSessionKind = static_cast<MediaMuxSessionKind>(255);
    MediaGraph invalidGraph;
    EXPECT_FALSE(ctx, MediaOutputSegmentBuilder::buildFileMuxOutput(invalidGraph, invalid));

    FileOutputSegmentOptions zeroStreams = missing;
    zeroStreams.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    MediaGraph zeroStreamsGraph;
    EXPECT_FALSE(ctx, MediaOutputSegmentBuilder::buildFileMuxOutput(
        zeroStreamsGraph, zeroStreams));

    FileOutputSegmentOptions explicitFfmpeg = missing;
    explicitFfmpeg.outputResourceKind = MediaOutputResourceKind::FFmpegFormatContext;
    explicitFfmpeg.muxSessionKind = MediaMuxSessionKind::FFmpegFile;
    explicitFfmpeg.expectVideo = true;
    MediaGraph graph;
    auto built = MediaOutputSegmentBuilder::buildFileMuxOutput(graph, explicitFfmpeg);
    EXPECT_TRUE(ctx, built);
    if (built) {
        const MediaNode* mux = graph.findNode(built.value().mux);
        const MediaNode* output = graph.findNode(built.value().fileOutput);
        EXPECT_TRUE(ctx, mux != nullptr);
        EXPECT_TRUE(ctx, output != nullptr);
        if (mux) {
            EXPECT_EQ(ctx,
                      mux->options.value(MediaTranscodeOptionKey::MuxSessionKind),
                      std::string("ffmpeg_file"));
        }
        if (output) {
            EXPECT_EQ(ctx,
                      output->options.value(MediaTranscodeOptionKey::OutputResourceKind),
                      std::string("ffmpeg_format_context"));
        }
    }

    FileOutputSegmentOptions mismatched = explicitFfmpeg;
    mismatched.outputResourceKind = MediaOutputResourceKind::ByteSink;
    MediaGraph mismatchedGraph;
    EXPECT_FALSE(ctx, MediaOutputSegmentBuilder::buildFileMuxOutput(
        mismatchedGraph, mismatched));
    EXPECT_TRUE(ctx, mismatchedGraph.nodes().empty());

    FileOutputSegmentOptions explicitProject = missing;
    explicitProject.outputResourceKind = MediaOutputResourceKind::ByteSink;
    explicitProject.muxSessionKind = MediaMuxSessionKind::ProjectMpegTs;
    explicitProject.expectVideo = true;
    explicitProject.expectAudio = true;
    MediaGraph projectGraph;
    auto project = MediaOutputSegmentBuilder::buildFileMuxOutput(
        projectGraph, explicitProject);
    EXPECT_TRUE(ctx, project);
    if (project) {
        const MediaPort* resource = projectGraph.findInputPort(
            project.value().mux, "resource");
        const MediaPort* planPort = projectGraph.findInputPort(
            project.value().mux, "plan");
        const MediaPort* codec = projectGraph.findInputPort(
            project.value().mux, "codec");
        const MediaPort* packet = projectGraph.findInputPort(
            project.value().mux, "packet");
        EXPECT_TRUE(ctx, resource != nullptr);
        EXPECT_TRUE(ctx, planPort != nullptr);
        EXPECT_TRUE(ctx, codec != nullptr);
        EXPECT_TRUE(ctx, packet != nullptr);
        if (resource) EXPECT_EQ(ctx, resource->payloadKind, MediaPayloadKind::OutputByteSink);
        if (planPort) EXPECT_EQ(ctx, planPort->payloadKind, MediaPayloadKind::TsMuxRuntimePlan);
        if (codec) EXPECT_EQ(ctx, codec->payloadKind, MediaPayloadKind::CodecContext);
        if (packet) EXPECT_EQ(ctx, packet->payloadKind, MediaPayloadKind::TsAccessUnit);
    }
}

void testLocalOutputPlannerOwnsMuxSessionDecision(TestContext& ctx)
{
    EXPECT_FALSE(ctx, MediaLocalFileOutputPlanner::plan({}, "mp4"));
    auto plan = MediaLocalFileOutputPlanner::plan("output.mp4", "mp4");
    EXPECT_TRUE(ctx, plan);
    if (plan) {
        EXPECT_TRUE(ctx, plan.value().muxSessionKind.has_value());
        EXPECT_TRUE(ctx, plan.value().outputResourceKind.has_value());
        if (plan.value().muxSessionKind) {
            EXPECT_EQ(ctx,
                      *plan.value().muxSessionKind,
                      MediaMuxSessionKind::FFmpegFile);
        }
        if (plan.value().outputResourceKind) {
            EXPECT_EQ(ctx,
                      *plan.value().outputResourceKind,
                      MediaOutputResourceKind::FFmpegFormatContext);
        }
    }
}

void testOutputResourceKindOptionMappingFailsClosed(TestContext& ctx)
{
    auto ffmpeg = mediaOutputResourceKindOptionValue(
        MediaOutputResourceKind::FFmpegFormatContext);
    EXPECT_TRUE(ctx, ffmpeg);
    if (ffmpeg) {
        EXPECT_EQ(ctx, ffmpeg.value(), std::string("ffmpeg_format_context"));
    }
    auto sink = mediaOutputResourceKindOptionValue(MediaOutputResourceKind::ByteSink);
    EXPECT_TRUE(ctx, sink);
    if (sink) EXPECT_EQ(ctx, sink.value(), std::string("byte_sink"));
    EXPECT_FALSE(ctx, mediaOutputResourceKindOptionValue(
        static_cast<MediaOutputResourceKind>(255)));
    EXPECT_FALSE(ctx, parseMediaOutputResourceKindOption(""));
    EXPECT_FALSE(ctx, parseMediaOutputResourceKindOption("unknown"));
}

void testMuxSessionKindOptionMappingFailsClosed(TestContext& ctx)
{
    auto ffmpeg = mediaMuxSessionKindOptionValue(MediaMuxSessionKind::FFmpegFile);
    EXPECT_TRUE(ctx, ffmpeg);
    if (ffmpeg) EXPECT_EQ(ctx, ffmpeg.value(), std::string("ffmpeg_file"));
    auto project = mediaMuxSessionKindOptionValue(MediaMuxSessionKind::ProjectMpegTs);
    EXPECT_TRUE(ctx, project);
    if (project) EXPECT_EQ(ctx, project.value(), std::string("project_mpegts"));
    EXPECT_FALSE(ctx, mediaMuxSessionKindOptionValue(
        static_cast<MediaMuxSessionKind>(255)));

    auto parsedFfmpeg = parseMediaMuxSessionKindOption("ffmpeg_file");
    EXPECT_TRUE(ctx, parsedFfmpeg);
    if (parsedFfmpeg) {
        EXPECT_EQ(ctx, parsedFfmpeg.value(), MediaMuxSessionKind::FFmpegFile);
    }
    auto parsedProject = parseMediaMuxSessionKindOption("project_mpegts");
    EXPECT_TRUE(ctx, parsedProject);
    if (parsedProject) {
        EXPECT_EQ(ctx, parsedProject.value(), MediaMuxSessionKind::ProjectMpegTs);
    }
    EXPECT_FALSE(ctx, parseMediaMuxSessionKindOption(""));
    EXPECT_FALSE(ctx, parseMediaMuxSessionKindOption("unknown"));
}

} // namespace

int main()
{
    TestContext ctx;
    MediaGraph graph;
    const auto node = graph.addNode(MediaNodeKind::DebugDump, "builder.node");
    EXPECT_TRUE(ctx, MediaGraphBuildSupport::setNodeOptionChecked(
                         graph, "builder test", node, "required.option", "value"));
    const MediaNode* built = graph.findNode(node);
    EXPECT_TRUE(ctx, built != nullptr);
    if (built) EXPECT_EQ(ctx, built->options.value("required.option"), std::string("value"));
    EXPECT_FALSE(ctx, MediaGraphBuildSupport::setNodeOptionChecked(
                          graph, "builder test", MediaNodeId::invalid(), "invalid", "value"));
    MediaPacketCopyBranchOptions packetCopy;
    EXPECT_FALSE(ctx, packetCopy.normalizePackets.has_value());
    static_assert(std::is_same_v<decltype(packetCopy.normalizePackets), std::optional<bool>>);
    packetCopy.normalizePackets = false;
    EXPECT_FALSE(ctx, *packetCopy.normalizePackets);
    packetCopy.normalizePackets = true;
    EXPECT_TRUE(ctx, *packetCopy.normalizePackets);
    MediaVideoBranchSegmentOptions videoSegment;
    EXPECT_FALSE(ctx, videoSegment.normalizePacketCopy.has_value());
    static_assert(std::is_same_v<decltype(videoSegment.normalizePacketCopy), std::optional<bool>>);
    MediaVideoPacketCopyBranchOptions videoCopy;
    EXPECT_FALSE(ctx, videoCopy.normalizePackets.has_value());
    static_assert(std::is_same_v<decltype(videoCopy.normalizePackets), std::optional<bool>>);
    MediaAudioBranchSegmentOptions audioSegment;
    EXPECT_FALSE(ctx, audioSegment.normalizeInputPackets.has_value());
    static_assert(std::is_same_v<decltype(audioSegment.normalizeInputPackets), std::optional<bool>>);
    MediaAudioEncodeBranchOptions audioEncode;
    EXPECT_FALSE(ctx, audioEncode.normalizePackets.has_value());
    static_assert(std::is_same_v<decltype(audioEncode.normalizePackets), std::optional<bool>>);
    EXPECT_FALSE(ctx, audioEncode.correctionMode.has_value());
    EXPECT_FALSE(ctx, audioEncode.correctionGeneration.has_value());
    testAudioCorrectionBuilderContract(ctx);
    testSynchronizedAudioPacketCopyIsUnsupported(ctx);
    testTypedPacketCopyEndpointsPreserveOwnerBoundaries(ctx);
    testFileMuxSessionKindIsExplicit(ctx);
    testLocalOutputPlannerOwnsMuxSessionDecision(ctx);
    testMuxSessionKindOptionMappingFailsClosed(ctx);
    testOutputResourceKindOptionMappingFailsClosed(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
