#include "common/TestAssert.h"

#include "internal/graph/builder/realtime/MediaRealtimeIngestToMuxGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimePacketRelayGraphBuilder.h"
#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/utils/MediaUrlUtils.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

std::string sampleVideoPath()
{
    return (std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
            "tests" /
            "samples" /
            "sample_h264_aac_2560x1440.mp4").string();
}

MediaRealtimeGraphBuilderOptions validRealtimeOptions()
{
    MediaRealtimeGraphBuilderOptions options;
    options.input.url = sampleVideoPath();
    options.output.host = "127.0.0.1";
    options.output.basePort = 5004;
    options.output.sdpPath = "realtime-test.sdp";
    options.parameters.execution.includeVideo = true;
    options.parameters.execution.includeAudio = false;
    options.parameters.execution.disableHardware = true;
    options.parameters.video.codecName = "h264";
    options.parameters.video.bFrames = 0;
    return options;
}

const MediaNode* findNodeByKind(const MediaGraph& graph, MediaNodeKind kind)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == kind) {
            return &node;
        }
    }
    return nullptr;
}

void testValidationRejectsMissingInput(TestContext& ctx)
{
    MediaRealtimeGraphBuilderOptions options = validRealtimeOptions();
    options.input.url.clear();

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testValidationRejectsUnsupportedRealtimeInput(TestContext& ctx)
{
    MediaRealtimeGraphBuilderOptions options = validRealtimeOptions();
    options.input.url = "rtp://127.0.0.1:5004";

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::Unsupported);
    }

    options.input.url = "camera.sdp";
    const auto sdpStatus = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, sdpStatus);
    if (!sdpStatus) {
        EXPECT_EQ(ctx, sdpStatus.error().code, media::ErrorCode::Unsupported);
    }
}

void testValidationRejectsOddRtpPort(TestContext& ctx)
{
    MediaRealtimeGraphBuilderOptions options = validRealtimeOptions();
    options.output.basePort = 5005;

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testLegacyRealtimeKindsAreUnsupported(TestContext& ctx)
{
    MediaRealtimeGraphBuilderOptions packetRelayOptions;
    packetRelayOptions.kind = MediaRealtimeGraphKind::PacketRelay;
    auto packetRelay = MediaRealtimeGraphBuilder::build(packetRelayOptions);
    EXPECT_FALSE(ctx, packetRelay);
    if (!packetRelay) {
        EXPECT_EQ(ctx, packetRelay.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeGraphBuilderOptions ingestOptions;
    ingestOptions.kind = MediaRealtimeGraphKind::IngestToMux;
    auto ingest = MediaRealtimeGraphBuilder::build(ingestOptions);
    EXPECT_FALSE(ctx, ingest);
    if (!ingest) {
        EXPECT_EQ(ctx, ingest.error().code, media::ErrorCode::Unsupported);
    }

    auto packetRelayDirect = MediaRealtimePacketRelayGraphBuilder::build(packetRelayOptions);
    EXPECT_FALSE(ctx, packetRelayDirect);
    if (!packetRelayDirect) {
        EXPECT_EQ(ctx, packetRelayDirect.error().code, media::ErrorCode::Unsupported);
    }

    auto ingestDirect = MediaRealtimeIngestToMuxGraphBuilder::build(ingestOptions);
    EXPECT_FALSE(ctx, ingestDirect);
    if (!ingestDirect) {
        EXPECT_EQ(ctx, ingestDirect.error().code, media::ErrorCode::Unsupported);
    }
}

void testBuildPlansVideoStreamAndSoftwareFallback(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRealtimeOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RealtimeInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::Demux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoEncode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpMux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpOutput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::SdpWriter) != nullptr);

    const MediaNode* encode = findNodeByKind(graph, MediaNodeKind::VideoEncode);
    EXPECT_TRUE(ctx, encode != nullptr);
    if (encode) {
        EXPECT_EQ(ctx, encode->options.value("pipeline.chain"), std::string("software"));
        EXPECT_EQ(ctx, encode->options.value("encoder"), std::string("libx264"));
    }
}

void expectGraphCompiles(TestContext& ctx, MediaGraph graph)
{
    const auto validation = MediaGraphValidation::validate(graph);
    EXPECT_TRUE(ctx, validation.ok());
    if (!validation.ok()) {
        for (const auto& issue : validation.issues) {
            std::cerr << "validation issue: " << issue.message
                      << " node=" << issue.nodeId.value
                      << " port=" << issue.portId.value
                      << " edge=" << issue.edgeId.value << '\n';
        }
        return;
    }

    MediaGraphRuntime runtime;
    const auto compileStatus = runtime.compile(std::move(graph));
    EXPECT_TRUE(ctx, compileStatus);
    if (!compileStatus) {
        std::cerr << compileStatus.error().describe() << '\n';
    }
}

void testRuntimeCompileSupportsSoftwareChain(TestContext& ctx)
{
    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRealtimeOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }
    expectGraphCompiles(ctx, std::move(graphResult).value());
}

void testRuntimeCompileSupportsAutoHardwareChain(TestContext& ctx)
{
    MediaRealtimeGraphBuilderOptions options = validRealtimeOptions();
    options.parameters.execution.disableHardware = false;

    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }
    expectGraphCompiles(ctx, std::move(graphResult).value());
}

void testUrlRedactionHidesUserInfo(TestContext& ctx)
{
    EXPECT_EQ(ctx,
              redactUrlUserInfo("rtsp://user:password@example.invalid:554/Streaming/Channels/302"),
              std::string("rtsp://<redacted>@example.invalid:554/Streaming/Channels/302"));
    EXPECT_EQ(ctx,
              redactUrlUserInfo("rtsp://example.invalid/Streaming/Channels/302"),
              std::string("rtsp://example.invalid/Streaming/Channels/302"));
}

} // namespace

int main()
{
    TestContext ctx;

    testValidationRejectsMissingInput(ctx);
    testValidationRejectsUnsupportedRealtimeInput(ctx);
    testValidationRejectsOddRtpPort(ctx);
    testUrlRedactionHidesUserInfo(ctx);
    testLegacyRealtimeKindsAreUnsupported(ctx);
    testBuildPlansVideoStreamAndSoftwareFallback(ctx);
    testRuntimeCompileSupportsSoftwareChain(ctx);
    testRuntimeCompileSupportsAutoHardwareChain(ctx);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " realtime graph test expectation(s) failed\n";
        return 1;
    }

    std::cout << "realtime graph tests passed\n";
    return 0;
}
