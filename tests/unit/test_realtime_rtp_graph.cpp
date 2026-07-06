#include "internal/graph/builder/realtime/MediaRealtimeGraphBuilder.h"
#include "internal/graph/core/MediaGraph.h"
#include "internal/graph/model/MediaNodeKind.h"

#include "common/TestAssert.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

using media_transcode::test::TestContext;
using namespace media::ffmpeg::graph;

bool hasNodeKind(const MediaGraph& graph, MediaNodeKind kind)
{
    return std::any_of(graph.nodes().begin(), graph.nodes().end(), [kind](const MediaNode& node) {
        return node.kind == kind;
    });
}

const MediaNode* findNodeKind(const MediaGraph& graph, MediaNodeKind kind)
{
    const auto it = std::find_if(graph.nodes().begin(), graph.nodes().end(), [kind](const MediaNode& node) {
        return node.kind == kind;
    });
    return it == graph.nodes().end() ? nullptr : &*it;
}

MediaRealtimeGraphBuilderOptions validPacketRelayOptions()
{
    MediaRealtimeGraphBuilderOptions options;
    options.kind = MediaRealtimeGraphKind::PacketRelay;
    options.inputUrl = "rtp://127.0.0.1:5004";
    options.outputUrl = "rtp://127.0.0.1:5006";
    options.sdpPath = "realtime-test.sdp";
    options.mediaId = "video-main";
    options.enablePacketFanout = true;
    options.enableSdpWriter = true;
    options.queueCapacity = 16;
    options.highWatermark = 10;
    options.criticalWatermark = 14;
    return options;
}

void testBuildsPacketRelayRtpShape(TestContext& ctx)
{
    const auto result = MediaRealtimeGraphBuilder::build(validPacketRelayOptions());

    EXPECT_TRUE(ctx, result);
    if (!result) {
        std::cerr << "packet relay graph build failed: " << result.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = result.value().graph;
    EXPECT_EQ(ctx, graph.nodeCount(), 4U);
    EXPECT_EQ(ctx, graph.edgeCount(), 3U);
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RealtimeInput));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::PacketFanout));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RtpOutput));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::SdpWriter));

    const MediaNode* input = findNodeKind(graph, MediaNodeKind::RealtimeInput);
    const MediaNode* output = findNodeKind(graph, MediaNodeKind::RtpOutput);
    const MediaNode* sdp = findNodeKind(graph, MediaNodeKind::SdpWriter);
    EXPECT_TRUE(ctx, input != nullptr);
    EXPECT_TRUE(ctx, output != nullptr);
    EXPECT_TRUE(ctx, sdp != nullptr);
    if (input != nullptr && output != nullptr && sdp != nullptr) {
        EXPECT_EQ(ctx, input->options.value("url"), std::string("rtp://127.0.0.1:5004"));
        EXPECT_EQ(ctx, output->options.value("url"), std::string("rtp://127.0.0.1:5006"));
        EXPECT_EQ(ctx, sdp->options.value("path"), std::string("realtime-test.sdp"));
        EXPECT_EQ(ctx, input->options.value("media_id"), std::string("video-main"));
    }
}

void testBuildsDirectPacketRelayWithoutFanout(TestContext& ctx)
{
    auto options = validPacketRelayOptions();
    options.enablePacketFanout = false;
    options.enableSdpWriter = false;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_TRUE(ctx, result);
    if (!result) {
        std::cerr << "direct relay graph build failed: " << result.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = result.value().graph;
    EXPECT_EQ(ctx, graph.nodeCount(), 2U);
    EXPECT_EQ(ctx, graph.edgeCount(), 1U);
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RealtimeInput));
    EXPECT_FALSE(ctx, hasNodeKind(graph, MediaNodeKind::PacketFanout));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RtpOutput));
}

void testBuildsIngestToRtpMuxShape(TestContext& ctx)
{
    auto options = validPacketRelayOptions();
    options.kind = MediaRealtimeGraphKind::IngestToMux;
    options.enableRtpMux = true;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_TRUE(ctx, result);
    if (!result) {
        std::cerr << "ingest-to-mux graph build failed: " << result.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = result.value().graph;
    EXPECT_EQ(ctx, graph.nodeCount(), 4U);
    EXPECT_EQ(ctx, graph.edgeCount(), 3U);
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RealtimeInput));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::PacketFanout));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RtpMux));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RtpOutput));
    EXPECT_FALSE(ctx, hasNodeKind(graph, MediaNodeKind::SdpWriter));
}

} // namespace

int main()
{
    TestContext ctx;

    testBuildsPacketRelayRtpShape(ctx);
    testBuildsDirectPacketRelayWithoutFanout(ctx);
    testBuildsIngestToRtpMuxShape(ctx);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " realtime graph test expectation(s) failed\n";
        return 1;
    }

    std::cout << "all realtime graph tests passed\n";
    return 0;
}
