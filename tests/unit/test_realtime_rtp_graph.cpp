#include "common/TestAssert.h"

#include "internal/graph/builder/realtime/MediaRealtimeRtpTranscodeGraphBuilder.h"
#include "internal/graph/core/MediaGraphValidation.h"
#include "internal/graph/model/MediaNodeKind.h"
#include "internal/graph/model/RealtimeStreamLayout.h"
#include "internal/graph/nodes/video/VideoMonotonicTimestamp.h"
#include "internal/graph/planner/realtime/MediaRealtimeRtpTranscodePlanner.h"
#include "internal/graph/runtime/MediaGraphRuntime.h"
#include "internal/graph/utils/MediaUrlUtils.h"
#include "../../tools/common/GraphCliSupport.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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

std::uint32_t testProcessId() noexcept
{
#ifdef _WIN32
    return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

std::uint16_t localMpegTsUdpPort(std::uint16_t offset) noexcept
{
    const std::uint32_t base = 20000 + ((testProcessId() * 17) % 20000);
    return static_cast<std::uint16_t>(base + offset);
}

std::uint16_t findAvailableUdpPort() noexcept
{
#ifdef _WIN32
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return localMpegTsUdpPort(0);
    }

    SOCKET socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) {
        WSACleanup();
        return localMpegTsUdpPort(0);
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        closesocket(socketHandle);
        WSACleanup();
        return localMpegTsUdpPort(0);
    }

    int addressLength = sizeof(address);
    std::uint16_t port = localMpegTsUdpPort(0);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0) {
        port = ntohs(address.sin_port);
    }

    closesocket(socketHandle);
    WSACleanup();
    return port;
#else
    const int socketHandle = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketHandle < 0) {
        return localMpegTsUdpPort(0);
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(socketHandle);
        return localMpegTsUdpPort(0);
    }

    socklen_t addressLength = sizeof(address);
    std::uint16_t port = localMpegTsUdpPort(0);
    if (getsockname(socketHandle, reinterpret_cast<sockaddr*>(&address), &addressLength) == 0) {
        port = ntohs(address.sin_port);
    }

    close(socketHandle);
    return port;
#endif
}

std::uint16_t mpegTsUdpInputPort() noexcept
{
    static const std::uint16_t port = findAvailableUdpPort();
    return port;
}

std::uint16_t mpegTsUdpOutputPort() noexcept
{
    const std::uint16_t inputPort = mpegTsUdpInputPort();
    if (inputPort <= 65533) {
        return static_cast<std::uint16_t>(inputPort + 2);
    }
    return localMpegTsUdpPort(2);
}

std::string mpegTsUdpInputUrl()
{
    return "udp://127.0.0.1:" + std::to_string(mpegTsUdpInputPort()) + "?fifo_size=1000000&overrun_nonfatal=1";
}

std::string mpegTsUdpOutputUrl()
{
    return "udp://127.0.0.1:" + std::to_string(mpegTsUdpOutputPort());
}

std::filesystem::path ffmpegExecutablePath()
{
#ifdef _WIN32
    const std::filesystem::path bundled = "D:/mabs/local64/bin-video/ffmpeg.exe";
    if (std::filesystem::exists(bundled)) {
        return bundled;
    }
#else
    const std::filesystem::path bundled = "/usr/bin/ffmpeg";
    if (std::filesystem::exists(bundled)) {
        return bundled;
    }
#endif
    return "ffmpeg";
}

#ifdef _WIN32
std::wstring widenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

class LocalMpegTsUdpSource final {
public:
    static ::media::Result<LocalMpegTsUdpSource> start()
    {
        const std::filesystem::path ffmpeg = ffmpegExecutablePath();
        const std::string command =
            "\"" + ffmpeg.string() + "\" -hide_banner -loglevel error -re -stream_loop -1 -i \"" +
            sampleVideoPath() + "\" -map 0:v:0 -map 0:a:0? -c copy -f mpegts \"udp://127.0.0.1:" +
            std::to_string(mpegTsUdpInputPort()) + "?pkt_size=1316\"";

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};
        std::wstring commandLine = widenAscii(command);

        if (!CreateProcessW(nullptr,
                            commandLine.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            CREATE_NO_WINDOW,
                            nullptr,
                            nullptr,
                            &startupInfo,
                            &processInfo)) {
            return ::media::Result<LocalMpegTsUdpSource>::failure(
                ::media::ErrorInfo::invalidArgument("Failed to start local FFmpeg MPEG-TS UDP source"));
        }

        CloseHandle(processInfo.hThread);
        LocalMpegTsUdpSource source(processInfo.hProcess);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        return ::media::Result<LocalMpegTsUdpSource>::success(std::move(source));
    }

    LocalMpegTsUdpSource(LocalMpegTsUdpSource&& other) noexcept
        : m_process(other.m_process)
    {
        other.m_process = nullptr;
    }

    LocalMpegTsUdpSource& operator=(LocalMpegTsUdpSource&& other) noexcept
    {
        if (this != &other) {
            stop();
            m_process = other.m_process;
            other.m_process = nullptr;
        }
        return *this;
    }

    LocalMpegTsUdpSource(const LocalMpegTsUdpSource&) = delete;
    LocalMpegTsUdpSource& operator=(const LocalMpegTsUdpSource&) = delete;

    ~LocalMpegTsUdpSource()
    {
        stop();
    }

private:
    explicit LocalMpegTsUdpSource(HANDLE process)
        : m_process(process)
    {
    }

    void stop() noexcept
    {
        if (!m_process) {
            return;
        }
        TerminateProcess(m_process, 0);
        WaitForSingleObject(m_process, 2000);
        CloseHandle(m_process);
        m_process = nullptr;
    }

    HANDLE m_process = nullptr;
};
#else
class LocalMpegTsUdpSource final {
public:
    static ::media::Result<LocalMpegTsUdpSource> start()
    {
        const std::filesystem::path ffmpeg = ffmpegExecutablePath();
        const std::string input = sampleVideoPath();
        const std::string output = "udp://127.0.0.1:" + std::to_string(mpegTsUdpInputPort()) + "?pkt_size=1316";
        const pid_t pid = fork();
        if (pid < 0) {
            return ::media::Result<LocalMpegTsUdpSource>::failure(
                ::media::ErrorInfo::invalidArgument("Failed to fork local FFmpeg MPEG-TS UDP source"));
        }
        if (pid == 0) {
            execlp(ffmpeg.string().c_str(),
                   ffmpeg.filename().string().c_str(),
                   "-hide_banner",
                   "-loglevel",
                   "error",
                   "-re",
                   "-stream_loop",
                   "-1",
                   "-i",
                   input.c_str(),
                   "-map",
                   "0:v:0",
                   "-map",
                   "0:a:0?",
                   "-c",
                   "copy",
                   "-f",
                   "mpegts",
                   output.c_str(),
                   static_cast<char*>(nullptr));
            _exit(127);
        }

        LocalMpegTsUdpSource source(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) == pid) {
            source.m_process = -1;
            return ::media::Result<LocalMpegTsUdpSource>::failure(
                ::media::ErrorInfo::invalidArgument("Local FFmpeg MPEG-TS UDP source exited before probe"));
        }
        return ::media::Result<LocalMpegTsUdpSource>::success(std::move(source));
    }

    LocalMpegTsUdpSource(LocalMpegTsUdpSource&& other) noexcept
        : m_process(other.m_process)
    {
        other.m_process = -1;
    }

    LocalMpegTsUdpSource& operator=(LocalMpegTsUdpSource&& other) noexcept
    {
        if (this != &other) {
            stop();
            m_process = other.m_process;
            other.m_process = -1;
        }
        return *this;
    }

    LocalMpegTsUdpSource(const LocalMpegTsUdpSource&) = delete;
    LocalMpegTsUdpSource& operator=(const LocalMpegTsUdpSource&) = delete;

    ~LocalMpegTsUdpSource()
    {
        stop();
    }

private:
    explicit LocalMpegTsUdpSource(pid_t process)
        : m_process(process)
    {
    }

    void stop() noexcept
    {
        if (m_process <= 0) {
            return;
        }
        kill(m_process, SIGTERM);
        waitpid(m_process, nullptr, 0);
        m_process = -1;
    }

    pid_t m_process = -1;
};
#endif

MediaRealtimeRtpTranscodeRequest validRealtimeOptions()
{
    MediaRealtimeRtpTranscodeRequest options;
    options.input.type = RealtimeInputType::Url;
    options.input.streamLayout = RealtimeInputStreamLayout::SessionDescribed;
    options.input.url = sampleVideoPath();
    options.input.rtspTransport = "tcp";
    options.input.openTimeoutMs = 5000;
    options.input.readTimeoutMs = 5000;
    options.input.analyzeDurationUs = 500000;
    options.input.probeSizeBytes = 512 * 1024;
    options.input.lowLatency = true;
    options.output.host = "127.0.0.1";
    options.output.basePort = 5004;
    options.output.streamLayout = RealtimeOutputStreamLayout::SeparateStreams;
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

std::string repositoryFile(const std::string& relativePath)
{
    return readTextFile(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) / relativePath);
}

void expectTextContains(TestContext& ctx, const std::string& text, const std::string& needle)
{
    EXPECT_TRUE(ctx, text.find(needle) != std::string::npos);
}

void expectTextNotContains(TestContext& ctx, const std::string& text, const std::string& needle)
{
    EXPECT_FALSE(ctx, text.find(needle) != std::string::npos);
}

MediaRealtimeRtpTranscodeRequest validRawRtpOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.type = RealtimeInputType::RtpPort;
    options.input.streamLayout = RealtimeInputStreamLayout::SeparateStreams;
    options.input.url.clear();
    options.input.videoRtp.url = "rtp://127.0.0.1:5004";
    options.input.videoRtp.codecName = "h264";
    options.input.videoRtp.payloadType = 96;
    options.input.videoRtp.clockRate = 90000;
    options.input.videoRtp.fmtp = "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032";
    return options;
}

MediaRealtimeRtpTranscodeRequest validMpegTsUdpOptions()
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.type = RealtimeInputType::MpegTsUdp;
    options.input.streamLayout = RealtimeInputStreamLayout::MuxedTransportStream;
    options.input.url = mpegTsUdpInputUrl();
    options.input.rtspTransport.clear();
    options.output.streamLayout = RealtimeOutputStreamLayout::MuxedTransportStream;
    options.output.url = mpegTsUdpOutputUrl();
    options.output.host.clear();
    options.output.basePort.reset();
    options.output.sdpPath.clear();
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

void testVideoToolsAreSplitIntoDedicatedTargets(TestContext& ctx)
{
    const std::string cmake = repositoryFile("CMakeLists.txt");

    expectTextContains(ctx, cmake, "TARGET_LOCAL_VIDEO_CLI");
    expectTextContains(ctx, cmake, "TARGET_REALTIME_VIDEO_CLI");
    expectTextContains(ctx, cmake, "tools/local_video_cli/main.cpp");
    expectTextContains(ctx, cmake, "tools/realtime_video_cli/main.cpp");
    expectTextNotContains(ctx, cmake, "TARGET_GRAPH_TRANSCODE_CLI");
    expectTextNotContains(ctx, cmake, "TARGET_REALTIME_RTP_INPUT_CLI");
    EXPECT_FALSE(ctx, std::filesystem::exists(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                              "tools" / "graph_transcode_cli" / "main.cpp"));
    EXPECT_FALSE(ctx, std::filesystem::exists(std::filesystem::path(MEDIA_TRANSCODE_SOURCE_DIR) /
                                              "tools" / "realtime_rtp_input_cli" / "main.cpp"));
}

void testVideoToolsRejectLegacyBusinessSwitches(TestContext& ctx)
{
    const std::filesystem::path sourceDir = MEDIA_TRANSCODE_SOURCE_DIR;
    const std::filesystem::path localTool = sourceDir / "tools" / "local_video_cli" / "main.cpp";
    const std::filesystem::path realtimeTool = sourceDir / "tools" / "realtime_video_cli" / "main.cpp";
    EXPECT_TRUE(ctx, std::filesystem::exists(localTool));
    EXPECT_TRUE(ctx, std::filesystem::exists(realtimeTool));

    const std::string localSource = readTextFile(localTool);
    const std::string realtimeSource = readTextFile(realtimeTool);
    const std::string commonSource = repositoryFile("tools/common/VideoCliTranscodeOptions.h");
    const std::string combined = localSource + "\n" + realtimeSource + "\n" + commonSource;

    expectTextNotContains(ctx, combined, "\"--mode\"");
    expectTextNotContains(ctx, combined, "\"--video\"");
    expectTextNotContains(ctx, combined, "\"--audio\"");
    expectTextNotContains(ctx, combined, "\"--enable-hw\"");
    expectTextContains(ctx, combined, "--disable-hw");
    expectTextContains(ctx, combined, "--no-audio");
}

void testGraphRejectsBehaviorDefaultImplementations(TestContext& ctx)
{
    const std::string plannerHeader = repositoryFile("src/internal/graph/planner/MediaPipelinePlanner.h");
    const std::string audioPlannerHeader = repositoryFile("src/internal/graph/planner/MediaAudioPipelinePlanner.h");
    const std::string audioPlanner = repositoryFile("src/internal/graph/planner/MediaAudioPipelinePlanner.cpp");
    const std::string encoderBuilder = repositoryFile("src/internal/graph/builder/codec/CodecResolverEncoderContextBuilder.cpp");

    expectTextContains(ctx, plannerHeader, "MediaPipelinePlannerOptions() = delete");
    expectTextNotContains(ctx, plannerHeader, "MediaPipelinePlannerOptions options = {}");
    expectTextContains(ctx, audioPlannerHeader, "MediaAudioPipelinePlannerOptions() = delete");
    expectTextNotContains(ctx, audioPlannerHeader, "bool includeAudio = true");
    expectTextNotContains(ctx, audioPlanner, "plan.reason = \"no_audio\"");
    expectTextNotContains(ctx, encoderBuilder, "defaultBufferSizeFromRate");
    expectTextNotContains(ctx, encoderBuilder, "default buffer size");
    expectTextNotContains(ctx, encoderBuilder, "rc_min_rate = encoderContext->bit_rate");
    expectTextNotContains(ctx, encoderBuilder, "rc_max_rate = encoderContext->bit_rate");
}

void testPlannerRejectsUnresolvedBehaviorOptions(TestContext& ctx)
{
    MediaInputVideoStreamInfo input;
    input.streamIndex = 0;
    input.codecName = "h264";
    input.width = 1920;
    input.height = 1080;

    MediaPipelinePlannerOptions missingHardwarePreference(true, false, false, true, true, true, false);
    const auto missingHardware = MediaPipelinePlanner::planVideoTranscodeKnownInput(
        input,
        "rtp://127.0.0.1:5004",
        missingHardwarePreference);
    EXPECT_FALSE(ctx, missingHardware);
    if (!missingHardware) {
        EXPECT_EQ(ctx, missingHardware.error().code, media::ErrorCode::InvalidArgument);
    }

    MediaPipelinePlannerOptions missingRealtimeOptions(true, false, false, true, true, true, false);
    missingRealtimeOptions.preferredHardware = "auto";
    const auto missingRealtime = MediaPipelinePlanner::planVideoTranscodeRealtimeUrl(
        "rtsp://127.0.0.1/live",
        missingRealtimeOptions);
    EXPECT_FALSE(ctx, missingRealtime);
    if (!missingRealtime) {
        EXPECT_EQ(ctx, missingRealtime.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testValidationRejectsUnsupportedRealtimeInput(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRealtimeOptions();
    options.input.url = "rtp://127.0.0.1:5004";
    options.input.type = RealtimeInputType::Url;

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

void testValidationRequiresExplicitRealtimeStreamClassification(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest missingInputType = validRealtimeOptions();
    missingInputType.input.type.reset();
    expectPlannerInvalidArgument(ctx, missingInputType);

    MediaRealtimeRtpTranscodeRequest missingInputLayout = validRealtimeOptions();
    missingInputLayout.input.streamLayout.reset();
    expectPlannerInvalidArgument(ctx, missingInputLayout);

    MediaRealtimeRtpTranscodeRequest missingOutputLayout = validRealtimeOptions();
    missingOutputLayout.output.streamLayout.reset();
    expectPlannerInvalidArgument(ctx, missingOutputLayout);
}

void testExistingRealtimeModesMapToExplicitLayouts(TestContext& ctx)
{
    const auto urlPlan = MediaRealtimeRtpTranscodePlanner::plan(validRealtimeOptions());
    EXPECT_TRUE(ctx, urlPlan);
    if (urlPlan) {
        EXPECT_EQ(ctx, urlPlan.value().inputType, RealtimeInputType::Url);
        EXPECT_EQ(ctx, urlPlan.value().inputLayout, RealtimeInputStreamLayout::SessionDescribed);
        EXPECT_EQ(ctx, urlPlan.value().outputLayout, RealtimeOutputStreamLayout::SeparateStreams);
    }

    const auto rtpPlan = MediaRealtimeRtpTranscodePlanner::plan(validRawRtpOptions());
    EXPECT_TRUE(ctx, rtpPlan);
    if (rtpPlan) {
        EXPECT_EQ(ctx, rtpPlan.value().inputType, RealtimeInputType::RtpPort);
        EXPECT_EQ(ctx, rtpPlan.value().inputLayout, RealtimeInputStreamLayout::SeparateStreams);
        EXPECT_EQ(ctx, rtpPlan.value().outputLayout, RealtimeOutputStreamLayout::SeparateStreams);
    }
}

void testUnsupportedRealtimeStreamCombinationsFailInPlanner(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest sdp = validRealtimeOptions();
    sdp.input.type = RealtimeInputType::Sdp;
    auto sdpPlan = MediaRealtimeRtpTranscodePlanner::plan(sdp);
    EXPECT_FALSE(ctx, sdpPlan);
    if (!sdpPlan) {
        EXPECT_EQ(ctx, sdpPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest externalTs = validMpegTsUdpOptions();
    externalTs.input.type = RealtimeInputType::ExternalMpegTsPacket;
    auto externalTsPlan = MediaRealtimeRtpTranscodePlanner::plan(externalTs);
    EXPECT_FALSE(ctx, externalTsPlan);
    if (!externalTsPlan) {
        EXPECT_EQ(ctx, externalTsPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest externalRtp = validRawRtpOptions();
    externalRtp.input.type = RealtimeInputType::ExternalRtpPacket;
    auto externalRtpPlan = MediaRealtimeRtpTranscodePlanner::plan(externalRtp);
    EXPECT_FALSE(ctx, externalRtpPlan);
    if (!externalRtpPlan) {
        EXPECT_EQ(ctx, externalRtpPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest bundled = validRawRtpOptions();
    bundled.input.streamLayout = RealtimeInputStreamLayout::BundledStream;
    auto bundledPlan = MediaRealtimeRtpTranscodePlanner::plan(bundled);
    EXPECT_FALSE(ctx, bundledPlan);
    if (!bundledPlan) {
        EXPECT_EQ(ctx, bundledPlan.error().code, media::ErrorCode::Unsupported);
    }

    MediaRealtimeRtpTranscodeRequest bundledOutput = validRawRtpOptions();
    bundledOutput.output.streamLayout = RealtimeOutputStreamLayout::BundledStream;
    auto bundledOutputPlan = MediaRealtimeRtpTranscodePlanner::plan(bundledOutput);
    EXPECT_FALSE(ctx, bundledOutputPlan);
    if (!bundledOutputPlan) {
        EXPECT_EQ(ctx, bundledOutputPlan.error().code, media::ErrorCode::Unsupported);
    }
}

void testMpegTsUdpRejectsNonUdpInputUrl(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validMpegTsUdpOptions();
    options.input.url = sampleVideoPath();

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
    if (!plan) {
        EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
    }
}

void testSeparateRtpOutputRejectsSingleOutputUrl(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.output.url = "udp://127.0.0.1:6000";
    options.output.host.clear();
    options.output.basePort.reset();

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
    if (!plan) {
        EXPECT_EQ(ctx, plan.error().code, media::ErrorCode::InvalidArgument);
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

    MediaRealtimeRtpTranscodeRequest missingPacketizationMode = options;
    missingPacketizationMode.input.videoRtp.fmtp =
        "sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==;profile-level-id=4D4032";
    expectPlannerInvalidArgument(ctx, missingPacketizationMode);

    MediaRealtimeRtpTranscodeRequest missingProfileLevelId = options;
    missingProfileLevelId.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets=Z01AMpWQAoALWwEQAAA+gAAOpghhA,aOuPIA==";
    expectPlannerInvalidArgument(ctx, missingProfileLevelId);

    MediaRealtimeRtpTranscodeRequest emptySpropParameterSets = options;
    emptySpropParameterSets.input.videoRtp.fmtp =
        "packetization-mode=1;sprop-parameter-sets= ;profile-level-id=4D4032";
    expectPlannerInvalidArgument(ctx, emptySpropParameterSets);
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

    MediaRealtimeRtpTranscodeRequest emptyHevcVps = options;
    emptyHevcVps.input.videoRtp.codecName = "hevc";
    emptyHevcVps.input.videoRtp.fmtp =
        "sprop-vps= ;sprop-sps=QgEBAWAAAAMAsAAAAwAAAwB4oAPAgBDlja5JMvA=;sprop-pps=RAHBcrRiQA==";
    expectPlannerInvalidArgument(ctx, emptyHevcVps);

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
        EXPECT_EQ(ctx, h264Plan.value().inputType, RealtimeInputType::RtpPort);
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

    EXPECT_EQ(ctx, hevcPlan.value().inputType, RealtimeInputType::RtpPort);
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
    EXPECT_EQ(ctx,
              plan.value().videoOutput.url,
              std::string("rtp://127.0.0.1:5004?localrtpport=0&localrtcpport=0"));
    EXPECT_EQ(ctx,
              plan.value().audioOutput.url,
              std::string("rtp://127.0.0.1:5006?localrtpport=0&localrtcpport=0"));
}

void testRawRtpInheritsSourceCodecsWhenTranscodeCodecsAreOmitted(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpAudioVideoOptions();
    options.parameters.video.codecName.clear();
    options.parameters.audio.codecName.clear();

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().videoPlan.inputCodecName, std::string("h264"));
    EXPECT_EQ(ctx, plan.value().videoPlan.outputCodecName, std::string("h264"));
    EXPECT_EQ(ctx, plan.value().audioPlan.sourceCodecName, std::string("aac"));
    EXPECT_EQ(ctx, plan.value().audioPlan.targetCodecName, std::string("aac"));
}

void testRawRtpRejectsUnknownSourceCodecWhenCodecIsNotExplicit(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.video.codecName.clear();
    options.input.videoRtp.codecName = "unknown-source-codec";

    const auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_FALSE(ctx, plan);
}

void testRealtimeNoResizeDoesNotScoreFilterStage(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().videoPlan.selected.score, 600);
}

void testRealtimeResizeScoresFilterStage(TestContext& ctx)
{
    MediaRealtimeRtpTranscodeRequest options = validRawRtpOptions();
    options.parameters.video.width = 1280;
    options.parameters.video.height = 720;

    auto plan = MediaRealtimeRtpTranscodePlanner::plan(options);
    EXPECT_TRUE(ctx, plan);
    if (!plan) {
        std::cerr << plan.error().describe() << '\n';
        return;
    }

    EXPECT_EQ(ctx, plan.value().videoPlan.selected.score, 900);
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
        EXPECT_EQ(ctx,
                  audioOutput->options.value("url"),
                  std::string("rtp://127.0.0.1:5006?localrtpport=0&localrtcpport=0"));
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

void testBuildPlansMpegTsUdpMuxedOutputGraph(TestContext& ctx)
{
    auto source = LocalMpegTsUdpSource::start();
    EXPECT_TRUE(ctx, source);
    if (!source) {
        std::cerr << source.error().describe() << '\n';
        return;
    }

    const auto graphResult = MediaRealtimeRtpTranscodeGraphBuilder::build(validMpegTsUdpOptions());
    EXPECT_TRUE(ctx, graphResult);
    if (!graphResult) {
        std::cerr << graphResult.error().describe() << '\n';
        return;
    }

    const MediaGraph& graph = graphResult.value();
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::RealtimeInput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::Demux) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::StreamSplit) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::FileOutput) != nullptr);
    EXPECT_TRUE(ctx, findNodeByKind(graph, MediaNodeKind::FileMux) != nullptr);
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpOutput), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::RtpMux), static_cast<std::size_t>(0));
    EXPECT_EQ(ctx, countNodesByKind(graph, MediaNodeKind::SdpWriter), static_cast<std::size_t>(0));

    const MediaNode* fileOutput = findNodeByKind(graph, MediaNodeKind::FileOutput);
    EXPECT_TRUE(ctx, fileOutput != nullptr);
    if (fileOutput) {
        EXPECT_EQ(ctx, fileOutput->options.value("url"), mpegTsUdpOutputUrl());
        EXPECT_EQ(ctx, fileOutput->options.value("format"), std::string("mpegts"));
    }
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

void testSyntheticTimestampsAdvanceAfterRealTimestamp(TestContext& ctx)
{
    const AVRational sourceTimeBase{ 1, 90000 };
    const AVRational frameStepTimeBase{ 1, 30 };

    const auto firstMissing = nextSyntheticTimestamp(AV_NOPTS_VALUE,
                                                     frameStepTimeBase,
                                                     sourceTimeBase);
    EXPECT_TRUE(ctx, firstMissing);
    if (firstMissing) {
        EXPECT_EQ(ctx, firstMissing.value(), 0);
    }

    const auto afterReal = nextSyntheticTimestamp(900000,
                                                  frameStepTimeBase,
                                                  sourceTimeBase);
    EXPECT_TRUE(ctx, afterReal);
    if (afterReal) {
        EXPECT_EQ(ctx, afterReal.value(), 903000);
    }

    const auto invalidTimeBase = nextSyntheticTimestamp(900000,
                                                        AVRational{ 0, 1 },
                                                        sourceTimeBase);
    EXPECT_FALSE(ctx, invalidTimeBase);
    if (!invalidTimeBase) {
        EXPECT_EQ(ctx, invalidTimeBase.error().code, media::ErrorCode::InvalidArgument);
    }
}

} // namespace

int main()
{
    TestContext ctx;

    testValidationRejectsMissingInput(ctx);
    testLegacyArchitectureFilesAreRemoved(ctx);
    testVideoToolsAreSplitIntoDedicatedTargets(ctx);
    testVideoToolsRejectLegacyBusinessSwitches(ctx);
    testGraphRejectsBehaviorDefaultImplementations(ctx);
    testPlannerRejectsUnresolvedBehaviorOptions(ctx);
    testValidationRejectsUnsupportedRealtimeInput(ctx);
    testValidationRequiresExplicitRealtimeStreamClassification(ctx);
    testExistingRealtimeModesMapToExplicitLayouts(ctx);
    testUnsupportedRealtimeStreamCombinationsFailInPlanner(ctx);
    testMpegTsUdpRejectsNonUdpInputUrl(ctx);
    testSeparateRtpOutputRejectsSingleOutputUrl(ctx);
    testRawRtpMissingMetadataFailsInPlanner(ctx);
    testRawRtpRejectsUnsupportedMetadata(ctx);
    testRawRtpPlansH264AndHevcInput(ctx);
    testRawRtpAudioEndpointRequiredWhenAudioEnabled(ctx);
    testRawRtpPlansAudioVideoInput(ctx);
    testRawRtpInheritsSourceCodecsWhenTranscodeCodecsAreOmitted(ctx);
    testRawRtpRejectsUnknownSourceCodecWhenCodecIsNotExplicit(ctx);
    testRealtimeNoResizeDoesNotScoreFilterStage(ctx);
    testRealtimeResizeScoresFilterStage(ctx);
    testRawRtpPlansOpusAudioInput(ctx);
    testValidationRejectsOddRtpPort(ctx);
    testValidationRejectsAudioRtpPortOverflow(ctx);
    testRealtimeNoAudioProbeDoesNotRequestAudio(ctx);
    testUrlRedactionHidesUserInfo(ctx);
    testTimestampRescaleBumpsQuantizedDuplicates(ctx);
    testTimestampRescaleRejectsInvalidBoundaryValues(ctx);
    testSyntheticTimestampsAdvanceAfterRealTimestamp(ctx);
    testBuildPlansVideoStreamAndSoftwareExecution(ctx);
    testBuildPlansRealtimeUrlAudioBranch(ctx);
    testBuildPlansRawRtpH264Graph(ctx);
    testBuildPlansRawRtpAudioVideoGraph(ctx);
    testBuildPlansMpegTsUdpMuxedOutputGraph(ctx);
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
