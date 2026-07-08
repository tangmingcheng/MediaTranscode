#include "common/TestAssert.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/utils/MediaUrlUtils.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

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

MediaRealtimeRtpTranscodeRequest validRealtimeOptions()
{
    MediaRealtimeRtpTranscodeRequest options;
    options.input.kind = MediaRealtimeInputKind::RealtimeUrl;
    options.input.url = sampleVideoPath();
    options.input.rtspTransport = "tcp";
    options.input.openTimeoutMs = 5000;
    options.input.readTimeoutMs = 5000;
    options.input.analyzeDurationUs = 500000;
    options.input.probeSizeBytes = 512 * 1024;
    options.input.lowLatency = true;
    options.output.host = "127.0.0.1";
    options.output.basePort = 5004;
    options.output.sdpPath = "realtime-test.sdp";
    options.output.packetSize = 1200;
    options.parameters.execution.includeVideo = true;
    options.parameters.execution.includeAudio = false;
    options.parameters.execution.disableHardware = true;
    options.parameters.queues.metadata = 1;
    options.parameters.queues.packet = 256;
    options.parameters.queues.frame = 128;
    options.parameters.queues.mux = 256;
    options.parameters.video.codecName = "h264";
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

const MediaNode* findNodeByName(const MediaGraph& graph, const std::string& name)
{
    for (const MediaNode& node : graph.nodes()) {
        if (node.name == name) {
            return &node;
        }
    }
    return nullptr;
}

std::size_t countNodesByKind(const MediaGraph& graph, MediaNodeKind kind)
{
    std::size_t count = 0;
    for (const MediaNode& node : graph.nodes()) {
        if (node.kind == kind) {
            ++count;
        }
    }
    return count;
}

std::size_t countOccurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = text.find(needle, offset)) != std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

const MediaEdge* findEdgeBetweenKinds(const MediaGraph& graph,
                                      MediaNodeKind fromKind,
                                      MediaNodeKind toKind,
                                      MediaEdgeKind edgeKind)
{
    for (const MediaEdge& edge : graph.edges()) {
        const MediaNode* from = graph.findNode(edge.from.nodeId);
        const MediaNode* to = graph.findNode(edge.to.nodeId);
        if (from && to && from->kind == fromKind && to->kind == toKind && edge.edgeKind == edgeKind) {
            return &edge;
        }
    }
    return nullptr;
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void expectPlannerInvalidArgument(TestContext& ctx, const MediaRealtimeRtpTranscodeRequest& options)
{
    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
    if (!plan) {
        EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
    }
}

MediaRealtimeRtpTranscodeRequest validRawRtpOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.kind = MediaRealtimeInputKind::RawRtp;
    options.input.url.clear();
    options.input.videoRtp.url = "rtp://127.0.0.1:5004";
    options.input.videoRtp.codecName = "h264";
    options.input.videoRtp.payloadType = 96;
    options.input.videoRtp.clockRate = 90000;
    options.input.videoRtp.fmtp = "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032";
    return options;
}

MediaRealtimeRtpTranscodeRequest validRawRtpAudioVideoOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.execution.includeAudio = true;
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;
    options.input.audioRtp.url = "rtp://127.0.0.1:5006";
    options.input.audioRtp.codecName = "aac";
    options.input.audioRtp.payloadType = 97;
    options.input.audioRtp.clockRate = 48000;
    options.input.audioRtp.channels = 2;
    options.input.audioRtp.fmtp = "profile-level-id=1;mode=AAC-hbr;config=1190;sizelength=13;indexlength=3;indexdeltalength=3";
    return options;
}

void testValidationRejectsMissingInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.url.clear();

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testLegacyArchitectureFilesAreRemoved(TestContext& ctx)
{
    const std::filesystem::path root(MEDIA_TRANSCODE_SOURCE_DIR);
    const std::vector<std::filesystem::path> legacyPaths = {
        "src/local",
        "src/realtime",
        "src/internal/core",
        "src/internal/input",
        "src/internal/output",
        "src/MediaTranscode.cpp",
        "tools/local_transcode_probe",
        "tools/realtime_probe",
        "examples/api",
        "include/media_transcode/LocalVideoTranscode.h",
        "include/media_transcode/MediaTranscode.h",
        "include/media_transcode/MediaTypes.h",
        "tests/unit/test_local_transcode_api.cpp",
        "tests/integration/test_local_transcode_integration.cpp",
        "tests/integration/test_local_transcode_missing_input.cpp",
        "tests/compile/test_public_headers.cpp"
    };

    for (const auto& legacyPath : legacyPaths) {
        const auto fullPath = root / legacyPath;
        EXPECT_FALSE(ctx, std::filesystem::exists(fullPath));
        if (std::filesystem::exists(fullPath)) {
            std::cerr << "legacy architecture path still exists: "
                      << fullPath.string() << '\n';
        }
    }

    const std::filesystem::path internalRoot = root / "src" / "internal";
    for (const auto& entry : std::filesystem::directory_iterator(internalRoot)) {
        EXPECT_EQ(ctx, entry.path().filename().string(), std::string("graph"));
        if (entry.path().filename() != "graph") {
            std::cerr << "non-graph internal source still exists: "
                      << entry.path().string() << '\n';
        }
    }
}

void testValidationRejectsUnsupportedRealtimeInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.url = "rtp://127.0.0.1:5004";
    options.input.kind = MediaRealtimeInputKind::RealtimeUrl;

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

    options.input.url = "udp://127.0.0.1:5004";
    const auto udpStatus = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, udpStatus);
    if (!udpStatus) {
        EXPECT_EQ(ctx, udpStatus.error().code, media::ErrorCode::Unsupported);
    }

    options.input.url = "sdp://camera";
    const auto sdpSchemeStatus = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, sdpSchemeStatus);
    if (!sdpSchemeStatus) {
        EXPECT_EQ(ctx, sdpSchemeStatus.error().code, media::ErrorCode::Unsupported);
    }
}

void testRawRtpMissingMetadataFailsInPlanner(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();

    MediaRealtimeRtpTranscodeRequest missingCodec = options;
    missingCodec.input.videoRtp.codecName.clear();
    expectPlannerInvalidArgument(ctx, missingCodec);

    MediaRealtimeRtpTranscodeRequest missingPayloadType = options;
    missingPayloadType.input.videoRtp.payloadType.reset();
    expectPlannerInvalidArgument(ctx, missingPayloadType);

    MediaRealtimeRtpTranscodeRequest missingClockRate = options;
    missingClockRate.input.videoRtp.clockRate.reset();
    expectPlannerInvalidArgument(ctx, missingClockRate);

    MediaRealtimeRtpTranscodeRequest missingVideoFmtp = options;
    missingVideoFmtp.input.videoRtp.fmtp.clear();
    expectPlannerInvalidArgument(ctx, missingVideoFmtp);
}

void testRawRtpRejectsUnsupportedMetadata(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();

    MediaRealtimeRtpTranscodeRequest unsupportedVideo = options;
    unsupportedVideo.input.videoRtp.codecName = "vp9";
    expectPlannerInvalidArgument(ctx, unsupportedVideo);

    MediaRealtimeRtpTranscodeRequest staticPayloadType = options;
    staticPayloadType.input.videoRtp.payloadType = 35;
    expectPlannerInvalidArgument(ctx, staticPayloadType);

    MediaRealtimeRtpTranscodeRequest invalidPayloadType = options;
    invalidPayloadType.input.videoRtp.payloadType = 128;
    expectPlannerInvalidArgument(ctx, invalidPayloadType);

    MediaRealtimeRtpTranscodeRequest invalidClockRate = options;
    invalidClockRate.input.videoRtp.clockRate = 48000;
    expectPlannerInvalidArgument(ctx, invalidClockRate);

    MediaRealtimeRtpTranscodeRequest missingPort = options;
    missingPort.input.videoRtp.url = "rtp://127.0.0.1";
    expectPlannerInvalidArgument(ctx, missingPort);

    MediaRealtimeRtpTranscodeRequest pathUrl = options;
    pathUrl.input.videoRtp.url = "rtp://127.0.0.1:5004/video";
    expectPlannerInvalidArgument(ctx, pathUrl);

    MediaRealtimeRtpTranscodeRequest queryUrl = options;
    queryUrl.input.videoRtp.url = "udp://127.0.0.1:5004?pkt_size=1200";
    expectPlannerInvalidArgument(ctx, queryUrl);

    MediaRealtimeRtpTranscodeRequest fragmentUrl = options;
    fragmentUrl.input.videoRtp.url = "rtp://127.0.0.1:5004#stream";
    expectPlannerInvalidArgument(ctx, fragmentUrl);

    MediaRealtimeRtpTranscodeRequest userInfoUrl = options;
    userInfoUrl.input.videoRtp.url = "rtp://user@127.0.0.1:5004";
    expectPlannerInvalidArgument(ctx, userInfoUrl);
}

void testRawRtpPlansH264AndHevcInput(TestContext& ctx)
{
    auto h264Options = validRawRtpOptions();
    const auto h264Plan = MediaRealtimeRtpTranscodePlanner::plan(h264Options);
    EXPECT_TRUE(ctx, h264Plan);
    if (h264Plan) {
        EXPECT_EQ(ctx, h264Plan.value().inputKind, MediaRealtimeInputKind::RawRtp);
        EXPECT_EQ(ctx, h264Plan.value().videoPlan.inputCodecName, std::string("h264"));
        EXPECT_EQ(ctx, h264Plan.value().videoPlan.sourceStreamIndex, 0);
    }

    auto hevcOptions = validRawRtpOptions();
    hevcOptions.input.videoRtp.codecName = "hevc";
    hevcOptions.input.videoRtp.fmtp = "sprop-vps=QAEMAf//AWAAAAMAsAAAAwAAAwB4;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA==";
    const auto hevcPlan = MediaRealtimeRtpTranscodePlanner::plan(hevcOptions);
    EXPECT_TRUE(ctx, hevcPlan);
    if (!hevcPlan) {
        std::cerr << hevcPlan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, hevcPlan.value().inputKind, MediaRealtimeInputKind::RawRtp);
    EXPECT_EQ(ctx, hevcPlan.value().videoPlan.inputCodecName, std::string("hevc"));
    EXPECT_TRUE(ctx, hevcPlan.value().input.sdpText.find("H265/90000") != std::string::npos);
}

void testRawRtpAudioEndpointRequiredWhenAudioEnabled(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest missingAudio = validRawRtpOptions();
    missingAudio.parameters.execution.includeAudio = true;
    missingAudio.parameters.audio.codecName = "aac";
    expectPlannerInvalidArgument(ctx, missingAudio);

    MediaRealtimeRtpTranscodeRequest missingFmtp = validRawRtpAudioVideoOptions();
    missingFmtp.input.audioRtp.fmtp.clear();
    expectPlannerInvalidArgument(ctx, missingFmtp);
}

void testRawRtpPlansAudioVideoInput(TestContext& ctx)
{
    auto options = validRawRtpAudioVideoOptions();
    options.input.audioRtp.url = "rtp://192.0.2.10:5006";

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_TRUE(ctx, plan.value().audioPlan.enabled);
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceStreamIndex, 1);
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceCodecName, std::string("aac"));
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=video 5004 RTP/AVP 96") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=audio 5006 RTP/AVP 97") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=video 5004 RTP/AVP 96\r\nc=IN IP4 127.0.0.1\r\n") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=audio 5006 RTP/AVP 97\r\nc=IN IP4 192.0.2.10\r\n") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("a=fmtp:97 ") != std::string::npos);
    EXPECT_EQ(ctx, plan.value().videoOutput.url, std::string("rtp://127.0.0.1:5004"));
    EXPECT_EQ(ctx, plan.value().audioOutput.url, std::string("rtp://127.0.0.1:5006"));
}

void testRawRtpPlansOpusAudioInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.audio.codecName = "opus";
    options.input.audioRtp.codecName = "opus";
    options.input.audioRtp.payloadType = 98;
    options.input.audioRtp.clockRate = 48000;
    options.input.audioRtp.channels = 2;
    options.input.audioRtp.fmtp.clear();

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_TRUE(ctx, plan.value().audioPlan.enabled);
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceCodecName, std::string("opus"));
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("m=audio 5006 RTP/AVP 98") != std::string::npos);
    EXPECT_TRUE(ctx, plan.value().input.sdpText.find("a=rtpmap:98 opus/48000/2") != std::string::npos);
    EXPECT_FALSE(ctx, plan.value().input.sdpText.find("a=fmtp:98 ") != std::string::npos);
}

void testValidationRejectsOddRtpPort(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.output.basePort = 5005;

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testValidationRejectsAudioRtpPortOverflow(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.parameters.execution.includeAudio = true;
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;
    options.output.basePort = 65534;

    const auto status = MediaRealtimeRtpTranscodeGraphBuilder::validate(options);
    EXPECT_FALSE(ctx, status);
    if (!status) {
        EXPECT_EQ(ctx, status.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testRealtimeNoAudioProbeDoesNotRequestAudio(TestContext& ctx)
{
    const auto header = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "planner" /
                                     "MediaPipelineCapabilityScanner.h");
    EXPECT_TRUE(ctx, header.find("detectRealtimeInputStreamInfo(") != std::string::npos);
    EXPECT_TRUE(ctx, header.find("bool includeAudio") != std::string::npos);

    const auto planner = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                      "src" /
                                      "internal" /
                                      "graph" /
                                      "planner" /
                                      "realtime" /
                                      "MediaRealtimeRtpTranscodePlanner.cpp");
    EXPECT_TRUE(ctx, planner.find("detectRealtimeInputStreamInfo(options.input.url,") != std::string::npos);
    EXPECT_TRUE(ctx, planner.find("audioRequested(options));") != std::string::npos);
}

void testBuildPlansVideoStreamAndSoftwareExecution(TestContext& ctx)
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

void testBuildPlansRealtimeUrlAudioBranch(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.parameters.execution.includeAudio = true;
    options.parameters.audio.codecName = "aac";
    options.parameters.audio.sampleRate = 48000;
    options.parameters.audio.channels = 2;

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(options);
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioResample) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioEncode) != nullptr);
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpOutput), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpMux), static_cast<std::size_t>(2));

    const MediaNode* audioOutput = findNodeByName(graph, "realtime.audio.rtp.output");
    EXPECT_TRUE(ctx, audioOutput != nullptr);
    if (audioOutput) {
        EXPECT_EQ(ctx, audioOutput->options.value("url"), std::string("rtp://127.0.0.1:5006"));
    }

    const MediaNode* sdpWriter = findNodeByKind(graph, MediaNodeKind::SdpWriter);
    EXPECT_TRUE(ctx, sdpWriter != nullptr);
    if (sdpWriter) {
        EXPECT_EQ(ctx, sdpWriter->options.value("sdp.expected_contexts"), std::string("2"));
    }
    const auto source = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "nodes" /
                                     "output" /
                                     "SdpWriterNode.cpp");
    EXPECT_EQ(ctx, countOccurrences(source, "av_sdp_create("), static_cast<std::size_t>(1));
    EXPECT_TRUE(ctx, source.find("contexts.data(), static_cast<int>(contexts.size())") != std::string::npos);
}

void testBuildPlansRawRtpH264Graph(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RawRtpInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::Demux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::PacketNormalize) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::VideoEncode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpMux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RtpOutput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::SdpWriter) != nullptr);
    const MediaNode* timestampNode = findNodeByKind(graph, MediaNodeKind::VideoTimestamp);
    EXPECT_TRUE(ctx, timestampNode != nullptr);
    if (timestampNode) {
        EXPECT_EQ(ctx, timestampNode->options.value(MediaTranscodeOptionKey::VideoSynthesizeMissingTimestamps), std::string("1"));
    }
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::RawRtpInput, MediaNodeKind::Demux, MediaEdgeKind::Metadata) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::Demux, MediaNodeKind::StreamSplit, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::StreamSplit, MediaNodeKind::PacketNormalize, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::PacketNormalize, MediaNodeKind::VideoDecode, MediaEdgeKind::InputPacket) != nullptr);
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::VideoEncode, MediaNodeKind::RtpMux, MediaEdgeKind::EncodedPacket) != nullptr);
}

void testBuildPlansRawRtpAudioVideoGraph(TestContext& ctx)
{
    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpAudioVideoOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RawRtpInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioDecode) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::AudioEncode) != nullptr);
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpOutput), static_cast<std::size_t>(2));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpMux), static_cast<std::size_t>(2));
    EXPECT_TRUE(ctx, findEdgeBetweenKinds(graph, MediaNodeKind::AudioEncode, MediaNodeKind::RtpMux, MediaEdgeKind::EncodedPacket) != nullptr);
}

void testRealtimeBuilderDoesNotOwnPlannerDecisions(TestContext& ctx)
{
    const auto source = readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                     "src" /
                                     "internal" /
                                     "graph" /
                                     "builder" /
                                     "realtime" /
                                     "MediaRealtimeRtpTranscodeGraphBuilder.cpp");
    EXPECT_FALSE(ctx, source.empty());
    EXPECT_FALSE(ctx, source.find("MediaPipelinePlanner::") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("MediaPipelinePlannerOptions") != std::string::npos);
    EXPECT_FALSE(ctx, source.find(std::string("MediaRealtime") + "EdgePolicy") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("isUnsupportedRealtimeInputUrl") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("preferredHardware") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("outputCodecName") != std::string::npos);
    EXPECT_FALSE(ctx, source.find("bFrames") != std::string::npos);
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

void testRuntimeCompileSupportsRawRtpChain(TestContext& ctx)
{
    auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validRawRtpOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }
    expectGraphCompiles(ctx, std::move(graphResult).value());
}

void testRuntimeCompileSupportsAutoHardwareChain(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
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

void testTimestampRescaleBumpsQuantizedDuplicates(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 90000 };
    const AVRational encoderTimeBase{ 1, 25 };

    const auto first = rescaleStrictlyIncreasingTimestamp(540000,
                                                          sourceTimeBase,
                                                          encoderTimeBase,
                                                          AV_NOPTS_VALUE);
    EXPECT_TRUE(ctx, first);
    if (!first) {
        return;
    }
    EXPECT_EQ(ctx, first.value(), 150);

    const auto duplicate = rescaleStrictlyIncreasingTimestamp(540010,
                                                              sourceTimeBase,
                                                              encoderTimeBase,
                                                              first.value());
    EXPECT_TRUE(ctx, duplicate);
    if (!duplicate) {
        return;
    }
    EXPECT_EQ(ctx, duplicate.value(), 151);

    const auto nextQuantized = rescaleStrictlyIncreasingTimestamp(543600,
                                                                  sourceTimeBase,
                                                                  encoderTimeBase,
                                                                  duplicate.value());
    EXPECT_TRUE(ctx, nextQuantized);
    if (nextQuantized) {
        EXPECT_EQ(ctx, nextQuantized.value(), 152);
    }
}

void testTimestampRescaleRejectsInvalidBoundaryValues(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 90000 };
    const AVRational encoderTimeBase{ 1, 25 };

    const auto invalidPts = rescaleStrictlyIncreasingTimestamp(AV_NOPTS_VALUE,
                                                               sourceTimeBase,
                                                               encoderTimeBase,
                                                               AV_NOPTS_VALUE);
    EXPECT_FALSE(ctx, invalidPts);
    if (!invalidPts) {
        EXPECT_EQ(ctx, invalidPts.error().code, media::ErrorCode::InvalidArgument);
    }

    const auto invalidTimeBase = rescaleStrictlyIncreasingTimestamp(540000,
                                                                    AVRational{ 0, 1 },
                                                                    encoderTimeBase,
                                                                    AV_NOPTS_VALUE);
    EXPECT_FALSE(ctx, invalidTimeBase);
    if (!invalidTimeBase) {
        EXPECT_EQ(ctx, invalidTimeBase.error().code, media::ErrorCode::InvalidArgument);
    }

    const auto maxLastPts = rescaleStrictlyIncreasingTimestamp(540000,
                                                               sourceTimeBase,
                                                               encoderTimeBase,
                                                               std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(ctx, maxLastPts);
    if (!maxLastPts) {
        EXPECT_EQ(ctx, maxLastPts.error().code, media::ErrorCode::InvalidArgument);
    }
}

} // namespace

int main()
{
    TestContext ctx;

    testValidationRejectsMissingInput(ctx);
    testLegacyArchitectureFilesAreRemoved(ctx);
    testValidationRejectsUnsupportedRealtimeInput(ctx);
    testRawRtpMissingMetadataFailsInPlanner(ctx);
    testRawRtpRejectsUnsupportedMetadata(ctx);
    testRawRtpPlansH264AndHevcInput(ctx);
    testRawRtpAudioEndpointRequiredWhenAudioEnabled(ctx);
    testRawRtpPlansAudioVideoInput(ctx);
    testRawRtpPlansOpusAudioInput(ctx);
    testValidationRejectsOddRtpPort(ctx);
    testValidationRejectsAudioRtpPortOverflow(ctx);
    testRealtimeNoAudioProbeDoesNotRequestAudio(ctx);
    testUrlRedactionHidesUserInfo(ctx);
    testTimestampRescaleBumpsQuantizedDuplicates(ctx);
    testTimestampRescaleRejectsInvalidBoundaryValues(ctx);
    testBuildPlansVideoStreamAndSoftwareExecution(ctx);
    testBuildPlansRealtimeUrlAudioBranch(ctx);
    testBuildPlansRawRtpH264Graph(ctx);
    testBuildPlansRawRtpAudioVideoGraph(ctx);
    testRealtimeBuilderDoesNotOwnPlannerDecisions(ctx);
    testRuntimeCompileSupportsSoftwareChain(ctx);
    testRuntimeCompileSupportsRawRtpChain(ctx);
    testRuntimeCompileSupportsAutoHardwareChain(ctx);

    if (ctx.failures != 0) {
        std::cerr << ctx.failures << " realtime graph test expectation(s) failed\n";
        return 1;
    }

    std::cout << "realtime graph tests passed\n";
    return 0;
}
