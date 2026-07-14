#include "common/TestAssert.h"

#include "internal/graph/builder/MediaGraphBuildSupport.h"
#include "internal/graph/builder/MediaPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaVideoBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaVideoPacketCopyBranchBuilder.h"
#include "internal/graph/builder/segments/MediaAudioBranchSegmentBuilder.h"
#include "internal/graph/builder/segments/MediaAudioEncodeBranchBuilder.h"

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
    options.plan.targetCodecName = "aac";
    options.plan.targetEncoderName = "aac";
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
    return ctx.failures == 0 ? 0 : 1;
}
