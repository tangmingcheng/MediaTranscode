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

std::size_t countNodeKind(const MediaGraph& graph, MediaNodeKind kind)
{
    return static_cast<std::size_t>(std::count_if(graph.nodes().begin(), graph.nodes().end(), [kind](const MediaNode& node) {
        return node.kind == kind;
    }));
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

MediaRealtimeGraphBuilderOptions validRtpTranscodeOptions()
{
    MediaRealtimeGraphBuilderOptions options;
    options.kind = MediaRealtimeGraphKind::RtpTranscode;
    options.input.url = "rtp://127.0.0.1:5004";
    options.inputUrl = options.input.url;
    options.input.videoStreamIndex = 0;
    options.output.host = "127.0.0.1";
    options.output.basePort = 5006;
    options.output.sdpPath = "realtime-rtp-test.sdp";
    options.sdpPath = options.output.sdpPath;
    options.mediaId = "video-main";
    options.includeVideo = true;
    options.includeAudio = false;
    options.parameters.execution.includeVideo = true;
    options.parameters.execution.includeAudio = false;
    options.parameters.execution.disableHardware = true;
    options.parameters.video.bFrames = 0;
    options.parameters.queues.metadata = 1;
    options.parameters.queues.packet = 16;
    options.parameters.queues.frame = 16;
    options.parameters.queues.mux = 16;
    options.queueCapacity = 16;
    options.highWatermark = 10;
    options.criticalWatermark = 14;
    return options;
}

MediaRtpCodecHint h264Hint()
{
    return MediaRtpCodecHint{
        MediaStreamKind::Video,
        "h264",
        96,
        90000,
        0,
        "packetization-mode=1"
    };
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

void testRejectsEmptyRealtimeRtpInput(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.input.url.clear();
    options.inputUrl.clear();

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_FALSE(ctx, result);
}

void testRejectsRawRtpWithoutSdpOrHints(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.input.mode = MediaRealtimeInputMode::RawRtp;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_FALSE(ctx, result);
}

void testBuildsRawRtpWithSdp(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.input.mode = MediaRealtimeInputMode::RawRtp;
    options.input.sdpPath = "input.sdp";

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_TRUE(ctx, result);
}

void testBuildsRawRtpWithCompleteHint(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.input.mode = MediaRealtimeInputMode::RawRtp;
    options.input.videoStreamIndex = -1;
    options.input.codecHints.push_back(h264Hint());

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_TRUE(ctx, result);
    if (!result) {
        return;
    }
    const MediaNode* input = findNodeKind(result.value().graph, MediaNodeKind::RealtimeInput);
    EXPECT_TRUE(ctx, input != nullptr);
    if (input != nullptr) {
        EXPECT_FALSE(ctx, input->options.value("input.sdp_text").empty());
    }
}

void testRejectsMissingVideoSourceStreamIndex(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.input.videoStreamIndex = -1;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_FALSE(ctx, result);
}

void testRejectsRawRtpHintWithoutInputPort(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.input.mode = MediaRealtimeInputMode::RawRtp;
    options.input.url = "rtp://127.0.0.1";
    options.inputUrl = options.input.url;
    options.input.videoStreamIndex = -1;
    options.input.codecHints.push_back(h264Hint());

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_FALSE(ctx, result);
}

void testRejectsRawRtpHintWithoutInputPortForLegacyRealtimeKinds(TestContext& ctx)
{
    auto packetRelay = validPacketRelayOptions();
    packetRelay.input.mode = MediaRealtimeInputMode::RawRtp;
    packetRelay.input.url = "rtp://127.0.0.1";
    packetRelay.inputUrl = packetRelay.input.url;
    packetRelay.input.codecHints.push_back(h264Hint());

    auto ingestToMux = packetRelay;
    ingestToMux.kind = MediaRealtimeGraphKind::IngestToMux;
    ingestToMux.enableRtpMux = true;

    EXPECT_FALSE(ctx, MediaRealtimeGraphBuilder::build(packetRelay));
    EXPECT_FALSE(ctx, MediaRealtimeGraphBuilder::build(ingestToMux));
}

void testRejectsInvalidRtpPort(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.output.basePort = 5007;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_FALSE(ctx, result);
}

void testRejectsMissingMediaBranch(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.includeVideo = false;
    options.includeAudio = false;
    options.parameters.execution.includeVideo = false;
    options.parameters.execution.includeAudio = false;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_FALSE(ctx, result);
}

void testBuildsVideoOnlyRealtimeRtpTranscodeShape(TestContext& ctx)
{
    const auto result = MediaRealtimeGraphBuilder::build(validRtpTranscodeOptions());

    EXPECT_TRUE(ctx, result);
    if (!result) {
        std::cerr << "realtime RTP transcode graph build failed: " << result.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = result.value().graph;
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RealtimeInput));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::Demux));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::StreamSplit));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::CodecResolver));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::VideoDecode));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::VideoEncode));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RtpMux));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::RtpOutput));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::SdpWriter));
    EXPECT_FALSE(ctx, hasNodeKind(graph, MediaNodeKind::AudioDecode));
    EXPECT_FALSE(ctx, hasNodeKind(graph, MediaNodeKind::AudioEncode));

    const MediaNode* input = findNodeKind(graph, MediaNodeKind::RealtimeInput);
    const MediaNode* output = findNodeKind(graph, MediaNodeKind::RtpOutput);
    const MediaNode* sdp = findNodeKind(graph, MediaNodeKind::SdpWriter);
    EXPECT_TRUE(ctx, input != nullptr);
    EXPECT_TRUE(ctx, output != nullptr);
    EXPECT_TRUE(ctx, sdp != nullptr);
    if (input != nullptr && output != nullptr && sdp != nullptr) {
        EXPECT_EQ(ctx, input->options.value("url"), std::string("rtp://127.0.0.1:5004"));
        EXPECT_EQ(ctx, input->options.value("input.mode"), std::string("url"));
        EXPECT_EQ(ctx, output->options.value("rtp.host"), std::string("127.0.0.1"));
        EXPECT_EQ(ctx, output->options.value("rtp.base_port"), std::string("5006"));
        EXPECT_EQ(ctx, sdp->options.value("path"), std::string("realtime-rtp-test.sdp"));
    }
    EXPECT_TRUE(ctx, graph.edgeCount() >= 8U);
}

void testBuildsAudioVideoRealtimeRtpTranscodeShape(TestContext& ctx)
{
    auto options = validRtpTranscodeOptions();
    options.includeAudio = true;
    options.parameters.execution.includeAudio = true;
    options.input.audioStreamIndex = 1;

    const auto result = MediaRealtimeGraphBuilder::build(options);

    EXPECT_TRUE(ctx, result);
    if (!result) {
        std::cerr << "audio/video realtime RTP transcode graph build failed: " << result.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = result.value().graph;
    EXPECT_EQ(ctx, countNodeKind(graph, MediaNodeKind::RtpOutput), 2U);
    EXPECT_EQ(ctx, countNodeKind(graph, MediaNodeKind::RtpMux), 2U);
    EXPECT_EQ(ctx, countNodeKind(graph, MediaNodeKind::SdpWriter), 1U);
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::VideoEncode));
    EXPECT_TRUE(ctx, hasNodeKind(graph, MediaNodeKind::AudioEncode));

    const MediaNode* sdp = findNodeKind(graph, MediaNodeKind::SdpWriter);
    EXPECT_TRUE(ctx, sdp != nullptr);
    if (sdp != nullptr) {
        EXPECT_EQ(ctx, sdp->options.value("sdp.expected_contexts"), std::string("2"));
    }
}

} // namespace

int main()
{
    TestContext ctx;

    testBuildsPacketRelayRtpShape(ctx);
    testBuildsDirectPacketRelayWithoutFanout(ctx);
    testBuildsIngestToRtpMuxShape(ctx);
    testRejectsEmptyRealtimeRtpInput(ctx);
    testRejectsRawRtpWithoutSdpOrHints(ctx);
    testBuildsRawRtpWithSdp(ctx);
    testBuildsRawRtpWithCompleteHint(ctx);
    testRejectsMissingVideoSourceStreamIndex(ctx);
    testRejectsRawRtpHintWithoutInputPort(ctx);
    testRejectsRawRtpHintWithoutInputPortForLegacyRealtimeKinds(ctx);
    testRejectsInvalidRtpPort(ctx);
    testRejectsMissingMediaBranch(ctx);
    testBuildsVideoOnlyRealtimeRtpTranscodeShape(ctx);
    testBuildsAudioVideoRealtimeRtpTranscodeShape(ctx);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " realtime graph test expectation(s) failed\n";
        return 1;
    }

    std::cout << "all realtime graph tests passed\n";
    return 0;
}
