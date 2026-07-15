#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
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
            "aac", "fltp", {}, {MediaAudioProfile::knownAacLow().ffmpegProfileId()}}});
    options.plan.resolvedOutput = std::move(resolved).value();
    options.normalizePackets = false;
    options.formatSourceNode = graph.addNode(MediaNodeKind::DebugDump, "format_source");
    graph.addOutputPort(options.formatSourceNode, "format", MediaStreamKind::Metadata,
                        MediaEdgeKind::Metadata, MediaPayloadKind::FormatContext);
    options.packetSourceNode = graph.addNode(MediaNodeKind::DebugDump, "packet_source");
    graph.addOutputPort(options.packetSourceNode, "audio", MediaStreamKind::Audio,
                        MediaEdgeKind::InputPacket, MediaPayloadKind::Packet);
    options.muxNode = graph.addNode(MediaNodeKind::DebugDump, "mux");
    graph.addInputPort(options.muxNode, "codec", MediaStreamKind::Audio,
                       MediaEdgeKind::Metadata, MediaPayloadKind::CodecContext);
    graph.addInputPort(options.muxNode, "packet", MediaStreamKind::Audio,
                       MediaEdgeKind::EncodedPacket, MediaPayloadKind::Packet);
    options.edgePolicies = MediaGraphBuildSupport::blockingEdgePolicySet(options.queues);
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
    MediaGraph incompleteGraph;
    auto incomplete = audioEncodeOptions(incompleteGraph);
    incomplete.plan.resolvedOutput.reset();
    incomplete.correctionMode = MediaAudioCorrectionExecutionMode::Disabled;
    EXPECT_FALSE(ctx, MediaAudioEncodeBranchBuilder::build(incompleteGraph, incomplete));
    EXPECT_TRUE(ctx, resampleNode(incompleteGraph) == nullptr);

    MediaGraph missingSourceGraph;
    auto missingSource = audioEncodeOptions(missingSourceGraph);
    missingSource.correctionMode = MediaAudioCorrectionExecutionMode::ExternalCorrectionRequired;
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
    external.correctionSourceNode = externalGraph.addNode(
        MediaNodeKind::DebugDump, "correction_source");
    external.correctionSourcePort = "correction";
    externalGraph.addOutputPort(
        external.correctionSourceNode, external.correctionSourcePort,
        MediaStreamKind::Audio, MediaEdgeKind::Event,
        MediaPayloadKind::GraphEvent);
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
        if (codec) EXPECT_EQ(ctx, codec->payloadKind, MediaPayloadKind::CodecParameters);
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
    testFileMuxSessionKindIsExplicit(ctx);
    testLocalOutputPlannerOwnsMuxSessionDecision(ctx);
    testMuxSessionKindOptionMappingFailsClosed(ctx);
    testOutputResourceKindOptionMappingFailsClosed(ctx);
    return ctx.failures == 0 ? 0 : 1;
}
